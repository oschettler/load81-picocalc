#include "picocalc_file_server.h"
#include "picocalc_fs_handler.h"
#include "picocalc_repl_handler.h"
#include "picocalc_debug_log.h"
#include "picocalc_framebuffer.h"
#include "debug.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lwip/err.h"
#include "fat32.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Client connection state */
typedef struct {
    struct tcp_pcb *pcb;
    bool active;
    char rx_buffer[FILE_SERVER_CMD_BUFFER_SIZE];
    uint16_t rx_len;
    char current_dir[256];
    uint32_t request_count;
    
    /* For binary data transfer (PUT command) */
    bool receiving_data;
    uint32_t data_expected;
    uint32_t data_received;
    uint8_t *data_buffer;
    char data_path[256];
} file_client_t;

/* Server state */
static struct {
    struct tcp_pcb *listen_pcb;
    file_client_t client;
    bool running;
    uint32_t total_requests;
    uint32_t total_connections;
} g_server;

/* Forward declarations */
static err_t file_accept(void *arg, struct tcp_pcb *newpcb, err_t err);
static err_t file_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static void file_err(void *arg, err_t err);
static void file_close_client(file_client_t *client);

/* Command handlers */
static void cmd_hello(file_client_t *client, const char *args);
static void cmd_pwd(file_client_t *client, const char *args);
static void cmd_cd(file_client_t *client, const char *args);
static void cmd_ls(file_client_t *client, const char *args);
static void cmd_cat(file_client_t *client, const char *args);
static void cmd_put(file_client_t *client, const char *args);
static void cmd_mkdir(file_client_t *client, const char *args);
static void cmd_rm(file_client_t *client, const char *args);
static void cmd_stat(file_client_t *client, const char *args);
static void cmd_repl(file_client_t *client, const char *args);
static void cmd_sshot(file_client_t *client, const char *args);
static void cmd_ping(file_client_t *client, const char *args);
static void cmd_quit(file_client_t *client, const char *args);

/* Command dispatch table */
typedef void (*cmd_handler_t)(file_client_t *client, const char *args);

typedef struct {
    const char *name;
    cmd_handler_t handler;
} command_entry_t;

static const command_entry_t commands[] = {
    {"HELLO", cmd_hello},
    {"PWD", cmd_pwd},
    {"CD", cmd_cd},
    {"LS", cmd_ls},
    {"CAT", cmd_cat},
    {"PUT", cmd_put},
    {"MKDIR", cmd_mkdir},
    {"RM", cmd_rm},
    {"STAT", cmd_stat},
    {"REPL", cmd_repl},
    {"SSHOT", cmd_sshot},
    {"PING", cmd_ping},
    {"QUIT", cmd_quit},
    {NULL, NULL}
};

/* Helper functions */
static void send_response(file_client_t *client, const char *response) {
    if (!client || !client->pcb || !response) return;
    
    size_t len = strlen(response);
    err_t err = tcp_write(client->pcb, response, len, TCP_WRITE_FLAG_COPY);
    if (err == ERR_OK) {
        tcp_output(client->pcb);
    }
}

static void send_ok(file_client_t *client, const char *data) {
    char response[FILE_SERVER_RESPONSE_BUFFER_SIZE];
    if (data) {
        snprintf(response, sizeof(response), "+OK %s\n", data);
    } else {
        snprintf(response, sizeof(response), "+OK\n");
    }
    send_response(client, response);
}

static void send_error(file_client_t *client, const char *message) {
    char response[FILE_SERVER_RESPONSE_BUFFER_SIZE];
    snprintf(response, sizeof(response), "-ERR %s\n", message ? message : "Unknown error");
    send_response(client, response);
}

static void send_data(file_client_t *client, const uint8_t *data, size_t len) {
    if (!client || !client->pcb || !data) {
        DEBUG_PRINTF("[FILE_SERVER] send_data: Invalid parameters\n");
        return;
    }
    
    DEBUG_PRINTF("[FILE_SERVER] send_data: Starting, len=%lu bytes\n", (unsigned long)len);
    
    /* Send +DATA header */
    char header[64];
    snprintf(header, sizeof(header), "+DATA %zu\n", len);
    DEBUG_PRINTF("[FILE_SERVER] send_data: Sending header: %s", header);
    send_response(client, header);
    
    /* Send binary data in chunks, respecting TCP send buffer */
    size_t sent = 0;
    while (sent < len) {
        /* Check available space in TCP send buffer */
        u16_t available = tcp_sndbuf(client->pcb);
        if (available == 0) {
            /* Send buffer full - flush and wait */
            DEBUG_PRINTF("[FILE_SERVER] send_data: Buffer full, flushing...\n");
            tcp_output(client->pcb);
            
            /* Poll network stack to process ACKs */
            for (int i = 0; i < 100 && tcp_sndbuf(client->pcb) == 0; i++) {
                cyw43_arch_poll();
                sleep_ms(1);
            }
            
            available = tcp_sndbuf(client->pcb);
            if (available == 0) {
                DEBUG_PRINTF("[FILE_SERVER] send_data: Timeout waiting for buffer space\n");
                break;
            }
        }
        
        /* Send chunk that fits in available buffer space */
        size_t chunk = len - sent;
        if (chunk > available) {
            chunk = available;
        }
        if (chunk > 1024) {  /* Limit chunk size to 1KB for better flow control */
            chunk = 1024;
        }
        
        DEBUG_PRINTF("[FILE_SERVER] send_data: Sending chunk at offset %lu, size %lu (available=%u)\n",
                    (unsigned long)sent, (unsigned long)chunk, available);
        
        err_t err = tcp_write(client->pcb, data + sent, chunk, TCP_WRITE_FLAG_COPY);
        if (err != ERR_OK) {
            DEBUG_PRINTF("[FILE_SERVER] send_data: tcp_write error %d at offset %lu\n",
                        err, (unsigned long)sent);
            break;
        }
        
        sent += chunk;
        DEBUG_PRINTF("[FILE_SERVER] send_data: Chunk queued, total sent=%lu\n", (unsigned long)sent);
        
        /* Flush every 4KB to avoid buffer buildup */
        if (sent % 4096 == 0) {
            tcp_output(client->pcb);
        }
    }
    
    /* Final flush */
    tcp_output(client->pcb);
    DEBUG_PRINTF("[FILE_SERVER] send_data: All data sent, sending +END marker\n");
    
    /* Send +END marker */
    send_response(client, "+END\n");
    
    DEBUG_PRINTF("[FILE_SERVER] send_data: Complete\n");
}

/* Parse and execute command */
static void parse_command(file_client_t *client, const char *line) {
    if (!line || !line[0]) return;
    
    DEBUG_PRINTF("[FILE_SERVER] Command: %s\n", line);
    
    /* Extract command name */
    char cmd[32];
    const char *args = NULL;
    const char *space = strchr(line, ' ');
    
    if (space) {
        size_t cmd_len = space - line;
        if (cmd_len >= sizeof(cmd)) cmd_len = sizeof(cmd) - 1;
        strncpy(cmd, line, cmd_len);
        cmd[cmd_len] = '\0';
        args = space + 1;
    } else {
        strncpy(cmd, line, sizeof(cmd) - 1);
        cmd[sizeof(cmd) - 1] = '\0';
    }
    
    /* Find and execute command */
    for (int i = 0; commands[i].name; i++) {
        if (strcmp(cmd, commands[i].name) == 0) {
            client->request_count++;
            g_server.total_requests++;
            commands[i].handler(client, args);
            return;
        }
    }
    
    /* Unknown command */
    send_error(client, "Unknown command");
}

/* Command implementations */
static void cmd_hello(file_client_t *client, const char *args) {
    send_ok(client, FILE_SERVER_PROTOCOL_VERSION);
}

static void cmd_pwd(file_client_t *client, const char *args) {
    send_ok(client, client->current_dir);
}

static void cmd_cd(file_client_t *client, const char *args) {
    if (!args || !args[0]) {
        /* No argument - go to root */
        strcpy(client->current_dir, "/");
        send_ok(client, NULL);
        return;
    }
    
    /* Normalize path */
    char new_path[256];
    fs_error_t err = fs_normalize_path(args, client->current_dir, new_path, sizeof(new_path));
    if (err != FS_OK) {
        send_error(client, fs_error_string(err));
        return;
    }
    
    /* Verify it's a directory by trying to open it */
    char *json;
    err = fs_list_dir(new_path, &json);
    if (err != FS_OK) {
        send_error(client, fs_error_string(err));
        return;
    }
    free(json);
    
    /* Update current directory */
    strncpy(client->current_dir, new_path, sizeof(client->current_dir) - 1);
    client->current_dir[sizeof(client->current_dir) - 1] = '\0';
    send_ok(client, NULL);
}

static void cmd_ls(file_client_t *client, const char *args) {
    /* Determine path */
    char path[256];
    if (args && args[0]) {
        fs_error_t err = fs_normalize_path(args, client->current_dir, path, sizeof(path));
        if (err != FS_OK) {
            send_error(client, fs_error_string(err));
            return;
        }
    } else {
        strncpy(path, client->current_dir, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
    }
    
    /* List directory */
    char *json;
    fs_error_t err = fs_list_dir(path, &json);
    if (err != FS_OK) {
        send_error(client, fs_error_string(err));
        return;
    }
    
    /* Send as data */
    send_data(client, (uint8_t *)json, strlen(json));
    free(json);
}

/* Context for chunked file reading */
typedef struct {
    file_client_t *client;
    uint32_t total_size;
    uint32_t bytes_sent;
    bool header_sent;
    bool error_occurred;
} cat_chunk_context_t;

/* Callback for chunked file reading */
static bool cat_chunk_callback(const uint8_t *chunk, size_t size, void *user_data) {
    cat_chunk_context_t *ctx = (cat_chunk_context_t *)user_data;
    
    /* Send header on first chunk */
    if (!ctx->header_sent) {
        /* If total_size is still 0, something went wrong - abort */
        if (ctx->total_size == 0) {
            ctx->error_occurred = true;
            return false;
        }
        
        char header[64];
        snprintf(header, sizeof(header), "+DATA %lu\n", (unsigned long)ctx->total_size);
        send_response(ctx->client, header);
        ctx->header_sent = true;
        ctx->bytes_sent = 0;  /* Reset counter after header */
    }
    
    /* Send chunk */
    if (ctx->client && ctx->client->pcb) {
        err_t err = tcp_write(ctx->client->pcb, chunk, size, TCP_WRITE_FLAG_COPY);
        if (err != ERR_OK) {
            ctx->error_occurred = true;
            return false;  /* Abort on error */
        }
        tcp_output(ctx->client->pcb);
        ctx->bytes_sent += size;
    } else {
        /* No client or PCB - connection lost */
        ctx->error_occurred = true;
        return false;
    }
    
    return true;  /* Continue */
}

static void cmd_cat(file_client_t *client, const char *args) {
    if (!args || !args[0]) {
        send_error(client, "Missing filename");
        return;
    }
    
    DEBUG_PRINTF("[FILE_SERVER] CAT command: args='%s'\n", args);
    
    /* Normalize path */
    char path[256];
    fs_error_t err = fs_normalize_path(args, client->current_dir, path, sizeof(path));
    if (err != FS_OK) {
        DEBUG_PRINTF("[FILE_SERVER] CAT: Path normalization failed: %s\n", fs_error_string(err));
        send_error(client, fs_error_string(err));
        return;
    }
    
    DEBUG_PRINTF("[FILE_SERVER] CAT: Normalized path='%s'\n", path);
    
    /* Get file size first */
    size_t file_size = 0;
    err = fs_get_file_size(path, &file_size);
    if (err != FS_OK) {
        DEBUG_PRINTF("[FILE_SERVER] CAT: fs_get_file_size failed: %s\n", fs_error_string(err));
        send_error(client, fs_error_string(err));
        return;
    }
    
    DEBUG_PRINTF("[FILE_SERVER] CAT: File size=%lu bytes\n", (unsigned long)file_size);
    
    /* Send +DATA header with size */
    char header[64];
    snprintf(header, sizeof(header), "+DATA %zu\n", file_size);
    DEBUG_PRINTF("[FILE_SERVER] CAT: Sending header: %s", header);
    send_response(client, header);
    
    /* Open file for streaming */
    fat32_file_t file;
    fat32_error_t fat_err = fat32_open(&file, path);
    if (fat_err != FAT32_OK) {
        DEBUG_PRINTF("[FILE_SERVER] CAT: fat32_open failed: %d\n", fat_err);
        return;
    }
    
    /* Stream file in chunks */
    uint8_t chunk_buffer[1024];  /* 1KB chunks */
    size_t total_sent = 0;
    
    while (total_sent < file_size) {
        size_t to_read = file_size - total_sent;
        if (to_read > sizeof(chunk_buffer)) {
            to_read = sizeof(chunk_buffer);
        }
        
        /* Read chunk from file */
        size_t bytes_read = 0;
        fat_err = fat32_read(&file, chunk_buffer, to_read, &bytes_read);
        if (fat_err != FAT32_OK || bytes_read == 0) {
            DEBUG_PRINTF("[FILE_SERVER] CAT: Read error at offset %lu\n", (unsigned long)total_sent);
            break;
        }
        
        /* Wait for TCP send buffer space */
        u16_t available = tcp_sndbuf(client->pcb);
        while (available < bytes_read && client->pcb) {
            tcp_output(client->pcb);
            cyw43_arch_poll();
            sleep_ms(1);
            available = tcp_sndbuf(client->pcb);
        }
        
        if (!client->pcb) {
            DEBUG_PRINTF("[FILE_SERVER] CAT: Connection lost\n");
            break;
        }
        
        /* Send chunk */
        err_t tcp_err = tcp_write(client->pcb, chunk_buffer, bytes_read, TCP_WRITE_FLAG_COPY);
        if (tcp_err != ERR_OK) {
            DEBUG_PRINTF("[FILE_SERVER] CAT: tcp_write error %d at offset %lu\n",
                        tcp_err, (unsigned long)total_sent);
            break;
        }
        
        total_sent += bytes_read;
        
        /* Flush every 4KB */
        if (total_sent % 4096 == 0) {
            tcp_output(client->pcb);
        }
        
        DEBUG_PRINTF("[FILE_SERVER] CAT: Sent %lu/%lu bytes\n",
                    (unsigned long)total_sent, (unsigned long)file_size);
    }
    
    /* Final flush */
    tcp_output(client->pcb);
    fat32_close(&file);
    
    DEBUG_PRINTF("[FILE_SERVER] CAT: Streaming complete, sent %lu bytes\n", (unsigned long)total_sent);
    
    /* Send +END marker */
    send_response(client, "+END\n");
    DEBUG_PRINTF("[FILE_SERVER] CAT: Command complete\n");
}

static void cmd_put(file_client_t *client, const char *args) {
    if (!args || !args[0]) {
        send_error(client, "Missing filename and size");
        return;
    }
    
    /* Parse: PUT path size */
    char path_arg[256];
    uint32_t size;
    if (sscanf(args, "%255s %lu", path_arg, (unsigned long *)&size) != 2) {
        send_error(client, "Invalid PUT syntax (use: PUT path size)");
        return;
    }
    
    /* Check size limit */
    if (size > FILE_SERVER_MAX_FILE_SIZE) {
        send_error(client, "File too large");
        return;
    }
    
    /* Normalize path */
    char path[256];
    fs_error_t err = fs_normalize_path(path_arg, client->current_dir, path, sizeof(path));
    if (err != FS_OK) {
        send_error(client, fs_error_string(err));
        return;
    }
    
    /* Allocate buffer for incoming data */
    client->data_buffer = malloc(size);
    if (!client->data_buffer) {
        send_error(client, "Out of memory");
        return;
    }
    
    /* Setup for receiving data */
    client->receiving_data = true;
    client->data_expected = size;
    client->data_received = 0;
    strncpy(client->data_path, path, sizeof(client->data_path) - 1);
    client->data_path[sizeof(client->data_path) - 1] = '\0';
    
    /* Send ready response */
    send_response(client, "+READY\n");
}

static void cmd_mkdir(file_client_t *client, const char *args) {
    if (!args || !args[0]) {
        send_error(client, "Missing directory name");
        return;
    }
    
    /* Normalize path */
    char path[256];
    fs_error_t err = fs_normalize_path(args, client->current_dir, path, sizeof(path));
    if (err != FS_OK) {
        send_error(client, fs_error_string(err));
        return;
    }
    
    /* Create directory */
    err = fs_mkdir(path);
    if (err != FS_OK) {
        send_error(client, fs_error_string(err));
        return;
    }
    
    send_ok(client, NULL);
}

static void cmd_rm(file_client_t *client, const char *args) {
    if (!args || !args[0]) {
        send_error(client, "Missing path");
        return;
    }
    
    /* Normalize path */
    char path[256];
    fs_error_t err = fs_normalize_path(args, client->current_dir, path, sizeof(path));
    if (err != FS_OK) {
        send_error(client, fs_error_string(err));
        return;
    }
    
    /* Delete */
    err = fs_delete(path);
    if (err != FS_OK) {
        send_error(client, fs_error_string(err));
        return;
    }
    
    send_ok(client, NULL);
}

static void cmd_stat(file_client_t *client, const char *args) {
    if (!args || !args[0]) {
        send_error(client, "Missing path");
        return;
    }
    
    /* Normalize path */
    char path[256];
    fs_error_t err = fs_normalize_path(args, client->current_dir, path, sizeof(path));
    if (err != FS_OK) {
        send_error(client, fs_error_string(err));
        return;
    }
    
    /* Get file info */
    char *json;
    err = fs_stat(path, &json);
    if (err != FS_OK) {
        send_error(client, fs_error_string(err));
        return;
    }
    
    /* Send as OK response with JSON data */
    send_ok(client, json);
    free(json);
}

static void cmd_repl(file_client_t *client, const char *args) {
    if (!args || !args[0]) {
        send_error(client, "Missing Lua code");
        return;
    }
    
    /* Execute Lua code */
    char *output;
    repl_error_t err = repl_execute(args, &output);
    if (err != REPL_OK) {
        send_error(client, repl_error_string(err));
        return;
    }
    
    /* Send output */
    send_ok(client, output);
    free(output);
}

static void cmd_sshot(file_client_t *client, const char *args) {
    DEBUG_PRINTF("[FILE_SERVER] SSHOT command\n");
    
    /* Calculate framebuffer size: 320x320 pixels * 2 bytes per pixel (RGB565) */
    const size_t fb_size = FB_WIDTH * FB_HEIGHT * 2;
    
    DEBUG_PRINTF("[FILE_SERVER] SSHOT: Framebuffer size=%lu bytes\n", (unsigned long)fb_size);
    
    /* Send +DATA header with size */
    char header[64];
    snprintf(header, sizeof(header), "+DATA %zu\n", fb_size);
    DEBUG_PRINTF("[FILE_SERVER] SSHOT: Sending header: %s", header);
    send_response(client, header);
    
    /* Stream framebuffer data in chunks */
    const uint8_t *fb_data = (const uint8_t *)g_fb.pixels;
    size_t total_sent = 0;
    
    while (total_sent < fb_size) {
        size_t to_send = fb_size - total_sent;
        if (to_send > 1024) {  /* 1KB chunks */
            to_send = 1024;
        }
        
        /* Wait for TCP send buffer space */
        u16_t available = tcp_sndbuf(client->pcb);
        while (available < to_send && client->pcb) {
            tcp_output(client->pcb);
            cyw43_arch_poll();
            sleep_ms(1);
            available = tcp_sndbuf(client->pcb);
        }
        
        if (!client->pcb) {
            DEBUG_PRINTF("[FILE_SERVER] SSHOT: Connection lost\n");
            return;
        }
        
        /* Send chunk */
        err_t tcp_err = tcp_write(client->pcb, fb_data + total_sent, to_send, TCP_WRITE_FLAG_COPY);
        if (tcp_err != ERR_OK) {
            DEBUG_PRINTF("[FILE_SERVER] SSHOT: tcp_write error %d at offset %lu\n",
                        tcp_err, (unsigned long)total_sent);
            return;
        }
        
        total_sent += to_send;
        
        /* Flush every 4KB */
        if (total_sent % 4096 == 0) {
            tcp_output(client->pcb);
        }
        
        DEBUG_PRINTF("[FILE_SERVER] SSHOT: Sent %lu/%lu bytes\n",
                    (unsigned long)total_sent, (unsigned long)fb_size);
    }
    
    /* Final flush */
    tcp_output(client->pcb);
    
    DEBUG_PRINTF("[FILE_SERVER] SSHOT: Streaming complete, sent %lu bytes\n", (unsigned long)total_sent);
    
    /* Send +END marker */
    send_response(client, "+END\n");
    DEBUG_PRINTF("[FILE_SERVER] SSHOT: Command complete\n");
}

static void cmd_ping(file_client_t *client, const char *args) {
    send_ok(client, NULL);
}

static void cmd_quit(file_client_t *client, const char *args) {
    send_ok(client, NULL);
    file_close_client(client);
}

/* TCP callbacks */
static err_t file_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    if (err != ERR_OK || !newpcb) {
        return ERR_VAL;
    }
    
    /* Check if we already have a client */
    if (g_server.client.active) {
        DEBUG_PRINTF("[FILE_SERVER] Rejecting connection - server busy\n");
        tcp_close(newpcb);
        return ERR_MEM;
    }
    
    DEBUG_PRINTF("[FILE_SERVER] ===== NEW CONNECTION ACCEPTED =====\n");
    
    /* Initialize client */
    memset(&g_server.client, 0, sizeof(file_client_t));
    g_server.client.active = true;
    g_server.client.pcb = newpcb;
    strcpy(g_server.client.current_dir, "/");
    
    g_server.total_connections++;
    
    /* Set TCP callbacks */
    tcp_arg(newpcb, &g_server.client);
    tcp_recv(newpcb, file_recv);
    tcp_err(newpcb, file_err);
    
    return ERR_OK;
}

static err_t file_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    file_client_t *client = (file_client_t *)arg;
    
    DEBUG_PRINTF("[FILE_SERVER] file_recv called, p=%p, err=%d\n", p, err);
    
    if (!p) {
        /* Connection closed */
        DEBUG_PRINTF("[FILE_SERVER] Connection closed by client\n");
        file_close_client(client);
        return ERR_OK;
    }
    
    if (err != ERR_OK) {
        pbuf_free(p);
        file_close_client(client);
        return err;
    }
    
    /* Handle binary data reception (PUT command) */
    if (client->receiving_data) {
        /* Copy data to buffer */
        uint16_t copy_len = p->tot_len;
        if (client->data_received + copy_len > client->data_expected) {
            copy_len = client->data_expected - client->data_received;
        }
        
        pbuf_copy_partial(p, client->data_buffer + client->data_received, copy_len, 0);
        client->data_received += copy_len;
        
        tcp_recved(tpcb, p->tot_len);
        pbuf_free(p);
        
        /* Check if we have all data */
        if (client->data_received >= client->data_expected) {
            /* Write file */
            fs_error_t fs_err = fs_write_file(client->data_path, 
                                             client->data_buffer, 
                                             client->data_expected);
            
            free(client->data_buffer);
            client->data_buffer = NULL;
            client->receiving_data = false;
            
            if (fs_err != FS_OK) {
                send_error(client, fs_error_string(fs_err));
            } else {
                send_ok(client, NULL);
            }
        }
        
        return ERR_OK;
    }
    
    /* Handle command reception */
    uint16_t copy_len = p->tot_len;
    if (client->rx_len + copy_len > sizeof(client->rx_buffer) - 1) {
        copy_len = sizeof(client->rx_buffer) - 1 - client->rx_len;
    }
    
    pbuf_copy_partial(p, client->rx_buffer + client->rx_len, copy_len, 0);
    client->rx_len += copy_len;
    client->rx_buffer[client->rx_len] = '\0';
    
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    
    /* Process complete lines */
    char *line_start = client->rx_buffer;
    char *line_end;
    
    DEBUG_PRINTF("[FILE_SERVER] Processing buffer, rx_len=%d\n", client->rx_len);
    
    while ((line_end = strchr(line_start, '\n')) != NULL) {
        *line_end = '\0';
        
        /* Remove \r if present */
        if (line_end > line_start && *(line_end - 1) == '\r') {
            *(line_end - 1) = '\0';
        }
        
        /* Process command */
        if (line_start[0] != '\0') {
            DEBUG_PRINTF("[FILE_SERVER] Calling parse_command with: '%s'\n", line_start);
            parse_command(client, line_start);
        }
        
        line_start = line_end + 1;
    }
    
    /* Move remaining data to start of buffer */
    if (line_start > client->rx_buffer) {
        size_t remaining = client->rx_len - (line_start - client->rx_buffer);
        if (remaining > 0) {
            memmove(client->rx_buffer, line_start, remaining);
        }
        client->rx_len = remaining;
        client->rx_buffer[client->rx_len] = '\0';
    }
    
    return ERR_OK;
}

static void file_err(void *arg, err_t err) {
    file_client_t *client = (file_client_t *)arg;
    DEBUG_PRINTF("[FILE_SERVER] TCP error: %d\n", err);
    if (client) {
        client->pcb = NULL;  /* PCB already freed by lwIP */
        file_close_client(client);
    }
}

static void file_close_client(file_client_t *client) {
    if (!client->active) {
        return;
    }
    
    DEBUG_PRINTF("[FILE_SERVER] Closing client connection\n");
    
    if (client->pcb) {
        tcp_arg(client->pcb, NULL);
        tcp_recv(client->pcb, NULL);
        tcp_err(client->pcb, NULL);
        tcp_close(client->pcb);
        client->pcb = NULL;
    }
    
    if (client->data_buffer) {
        free(client->data_buffer);
        client->data_buffer = NULL;
    }
    
    client->active = false;
}

/* Public API */
bool file_server_init(void) {
    memset(&g_server, 0, sizeof(g_server));
    
    /* Initialize subsystems */
    fs_error_t fs_err = fs_init();
    if (fs_err != FS_OK) {
        DEBUG_PRINTF("[FILE_SERVER] Failed to initialize filesystem: %s\n", 
                    fs_error_string(fs_err));
        return false;
    }
    
    repl_error_t repl_err = repl_init();
    if (repl_err != REPL_OK) {
        DEBUG_PRINTF("[FILE_SERVER] Failed to initialize REPL: %s\n",
                    repl_error_string(repl_err));
        return false;
    }
    
    DEBUG_PRINTF("[FILE_SERVER] Initialized\n");
    return true;
}

bool file_server_start(void) {
    if (g_server.running) {
        return false;
    }
    
    /* Create TCP listening socket */
    g_server.listen_pcb = tcp_new();
    if (!g_server.listen_pcb) {
        DEBUG_PRINTF("[FILE_SERVER] Failed to create TCP PCB\n");
        return false;
    }
    
    /* Bind to port */
    err_t err = tcp_bind(g_server.listen_pcb, IP_ADDR_ANY, FILE_SERVER_PORT);
    if (err != ERR_OK) {
        DEBUG_PRINTF("[FILE_SERVER] Failed to bind to port %d: %d\n", 
                    FILE_SERVER_PORT, err);
        tcp_close(g_server.listen_pcb);
        g_server.listen_pcb = NULL;
        return false;
    }
    
    /* Start listening */
    g_server.listen_pcb = tcp_listen(g_server.listen_pcb);
    if (!g_server.listen_pcb) {
        DEBUG_PRINTF("[FILE_SERVER] Failed to listen\n");
        return false;
    }
    
    /* Set accept callback */
    tcp_accept(g_server.listen_pcb, file_accept);
    
    g_server.running = true;
    DEBUG_PRINTF("[FILE_SERVER] Started on port %d\n", FILE_SERVER_PORT);
    
    return true;
}

void file_server_stop(void) {
    if (!g_server.running) {
        return;
    }
    
    DEBUG_PRINTF("[FILE_SERVER] Stopping\n");
    
    /* Close client */
    if (g_server.client.active) {
        file_close_client(&g_server.client);
    }
    
    /* Close listening socket */
    if (g_server.listen_pcb) {
        tcp_close(g_server.listen_pcb);
        g_server.listen_pcb = NULL;
    }
    
    g_server.running = false;
}

bool file_server_is_running(void) {
    return g_server.running;
}

void file_server_get_stats(uint32_t *total_connections, 
                          uint32_t *total_requests,
                          uint32_t *active_clients) {
    if (total_connections) *total_connections = g_server.total_connections;
    if (total_requests) *total_requests = g_server.total_requests;
    if (active_clients) *active_clients = g_server.client.active ? 1 : 0;
}