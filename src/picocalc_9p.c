/**
 * @file picocalc_9p.c
 * @brief 9P2000.u Protocol Server Implementation
 * 
 * Core server infrastructure providing TCP server, client management,
 * and message dispatch for the 9P filesystem protocol.
 */

#include "picocalc_9p.h"
#include "pico/stdlib.h"
#include "pico/sync.h"
#include "lwip/tcp.h"
#include "lwip/err.h"
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * Global Server State
 * ======================================================================== */

static struct {
    p9_server_state_t state;
    struct tcp_pcb *listen_pcb;
    p9_client_t clients[P9_MAX_CLIENTS];
    p9_server_stats_t stats;
    mutex_t mutex;
} g_server;

/* ========================================================================
 * Forward Declarations
 * ======================================================================== */

static err_t p9_tcp_accept(void *arg, struct tcp_pcb *newpcb, err_t err);
static err_t p9_tcp_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static void p9_tcp_err(void *arg, err_t err);
static err_t p9_tcp_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
static void p9_client_close(p9_client_t *client);
static void p9_process_message(p9_client_t *client);
static void p9_send_error(p9_client_t *client, uint16_t tag, const char *ename);

/* ========================================================================
 * Server Lifecycle Functions
 * ======================================================================== */

bool p9_server_init(void) {
    memset(&g_server, 0, sizeof(g_server));
    g_server.state = P9_SERVER_STATE_STOPPED;
    mutex_init(&g_server.mutex);
    
    /* Initialize all client structures */
    for (int i = 0; i < P9_MAX_CLIENTS; i++) {
        g_server.clients[i].active = false;
        p9_fid_table_init(&g_server.clients[i].fid_table);
    }
    
    return true;
}

bool p9_server_start(void) {
    mutex_enter_blocking(&g_server.mutex);
    
    if (g_server.state != P9_SERVER_STATE_STOPPED) {
        mutex_exit(&g_server.mutex);
        return false;
    }
    
    g_server.state = P9_SERVER_STATE_STARTING;
    
    /* Create TCP listening socket */
    g_server.listen_pcb = tcp_new();
    if (!g_server.listen_pcb) {
        g_server.state = P9_SERVER_STATE_ERROR;
        mutex_exit(&g_server.mutex);
        return false;
    }
    
    /* Bind to 9P port */
    err_t err = tcp_bind(g_server.listen_pcb, IP_ADDR_ANY, P9_SERVER_PORT);
    if (err != ERR_OK) {
        tcp_close(g_server.listen_pcb);
        g_server.listen_pcb = NULL;
        g_server.state = P9_SERVER_STATE_ERROR;
        mutex_exit(&g_server.mutex);
        return false;
    }
    
    /* Start listening */
    g_server.listen_pcb = tcp_listen(g_server.listen_pcb);
    if (!g_server.listen_pcb) {
        g_server.state = P9_SERVER_STATE_ERROR;
        mutex_exit(&g_server.mutex);
        return false;
    }
    
    /* Set accept callback */
    tcp_accept(g_server.listen_pcb, p9_tcp_accept);
    
    g_server.state = P9_SERVER_STATE_RUNNING;
    mutex_exit(&g_server.mutex);
    
    return true;
}

void p9_server_stop(void) {
    mutex_enter_blocking(&g_server.mutex);
    
    if (g_server.state != P9_SERVER_STATE_RUNNING) {
        mutex_exit(&g_server.mutex);
        return;
    }
    
    g_server.state = P9_SERVER_STATE_STOPPING;
    
    /* Close all client connections */
    for (int i = 0; i < P9_MAX_CLIENTS; i++) {
        if (g_server.clients[i].active) {
            p9_client_close(&g_server.clients[i]);
        }
    }
    
    /* Close listening socket */
    if (g_server.listen_pcb) {
        tcp_close(g_server.listen_pcb);
        g_server.listen_pcb = NULL;
    }
    
    g_server.state = P9_SERVER_STATE_STOPPED;
    mutex_exit(&g_server.mutex);
}

void p9_server_poll(void) {
    /* lwIP handles polling internally, nothing needed here */
}

p9_server_state_t p9_server_get_state(void) {
    return g_server.state;
}

void p9_server_get_stats(p9_server_stats_t *stats) {
    mutex_enter_blocking(&g_server.mutex);
    memcpy(stats, &g_server.stats, sizeof(p9_server_stats_t));
    mutex_exit(&g_server.mutex);
}

bool p9_server_is_running(void) {
    return g_server.state == P9_SERVER_STATE_RUNNING;
}

uint32_t p9_server_get_client_count(void) {
    uint32_t count = 0;
    mutex_enter_blocking(&g_server.mutex);
    for (int i = 0; i < P9_MAX_CLIENTS; i++) {
        if (g_server.clients[i].active) {
            count++;
        }
    }
    mutex_exit(&g_server.mutex);
    return count;
}

/* ========================================================================
 * TCP Callback Functions
 * ======================================================================== */

static err_t p9_tcp_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    if (err != ERR_OK || !newpcb) {
        return ERR_VAL;
    }
    
    /* Find free client slot */
    p9_client_t *client = NULL;
    mutex_enter_blocking(&g_server.mutex);
    
    for (int i = 0; i < P9_MAX_CLIENTS; i++) {
        if (!g_server.clients[i].active) {
            client = &g_server.clients[i];
            break;
        }
    }
    
    if (!client) {
        mutex_exit(&g_server.mutex);
        tcp_close(newpcb);
        return ERR_MEM;
    }
    
    /* Initialize client */
    memset(client, 0, sizeof(p9_client_t));
    client->active = true;
    client->state = P9_CLIENT_STATE_CONNECTED;
    client->pcb = newpcb;
    client->max_msg_size = P9_MAX_MSG_SIZE;
    p9_fid_table_init(&client->fid_table);
    
    /* Update statistics */
    g_server.stats.total_connections++;
    g_server.stats.active_connections++;
    
    mutex_exit(&g_server.mutex);
    
    /* Set TCP callbacks */
    tcp_arg(newpcb, client);
    tcp_recv(newpcb, p9_tcp_recv);
    tcp_err(newpcb, p9_tcp_err);
    tcp_sent(newpcb, p9_tcp_sent);
    
    return ERR_OK;
}

static err_t p9_tcp_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    p9_client_t *client = (p9_client_t *)arg;
    
    /* Check for errors first */
    if (err != ERR_OK) {
        if (p) {
            pbuf_free(p);
        }
        p9_client_close(client);
        return err;
    }
    
    /* p=NULL with err=ERR_OK means connection closed gracefully by remote */
    if (!p) {
        p9_client_close(client);
        return ERR_OK;
    }
    
    /* Copy data to client buffer */
    struct pbuf *q = p;
    uint32_t offset = client->rx_len;
    
    while (q && offset < P9_MAX_MSG_SIZE) {
        uint32_t copy_len = q->len;
        if (offset + copy_len > P9_MAX_MSG_SIZE) {
            copy_len = P9_MAX_MSG_SIZE - offset;
        }
        
        memcpy(client->rx_buffer + offset, q->payload, copy_len);
        offset += copy_len;
        q = q->next;
    }
    
    client->rx_len = offset;
    
    /* Acknowledge received data */
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    
    /* Process complete messages */
    while (client->rx_len >= 4) {
        /* Read message size (first 4 bytes, little-endian) */
        uint32_t msg_size = client->rx_buffer[0] |
                           (client->rx_buffer[1] << 8) |
                           (client->rx_buffer[2] << 16) |
                           (client->rx_buffer[3] << 24);
        
        if (msg_size > P9_MAX_MSG_SIZE || msg_size < 7) {
            /* Invalid message size */
            p9_client_close(client);
            return ERR_VAL;
        }
        
        if (client->rx_len < msg_size) {
            /* Incomplete message, wait for more data */
            break;
        }
        
        /* Process complete message */
        p9_process_message(client);
        
        /* Remove processed message from buffer */
        if (client->rx_len > msg_size) {
            memmove(client->rx_buffer, client->rx_buffer + msg_size,
                   client->rx_len - msg_size);
        }
        client->rx_len -= msg_size;
    }
    
    return ERR_OK;
}

static void p9_tcp_err(void *arg, err_t err) {
    p9_client_t *client = (p9_client_t *)arg;
    if (client) {
        client->pcb = NULL;  /* PCB already freed by lwIP */
        p9_client_close(client);
    }
}

static err_t p9_tcp_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    /* Data acknowledged by client */
    return ERR_OK;
}

/* ========================================================================
 * Client Management
 * ======================================================================== */

static void p9_client_close(p9_client_t *client) {
    if (!client->active) {
        return;
    }
    
    mutex_enter_blocking(&g_server.mutex);
    
    /* Close all FIDs */
    p9_fid_free_all(&client->fid_table);
    
    /* Close TCP connection */
    if (client->pcb) {
        tcp_arg(client->pcb, NULL);
        tcp_recv(client->pcb, NULL);
        tcp_err(client->pcb, NULL);
        tcp_sent(client->pcb, NULL);
        tcp_close(client->pcb);
        client->pcb = NULL;
    }
    
    /* Mark client as inactive */
    client->active = false;
    
    /* Update statistics */
    if (g_server.stats.active_connections > 0) {
        g_server.stats.active_connections--;
    }
    
    mutex_exit(&g_server.mutex);
}

/* ========================================================================
 * Message Processing
 * ======================================================================== */

static void p9_process_message(p9_client_t *client) {
    p9_msg_t req, resp;
    
    /* Initialize request message from buffer - this reads the header */
    p9_msg_init_read(&req, client->rx_buffer, client->rx_len);
    
    /* Use header values already read by p9_msg_init_read */
    uint8_t type = req.type;
    uint16_t tag = req.tag;
    
    /* Check if message type is valid before writing response header */
    bool valid_type = false;
    switch (type) {
        case Tversion:
        case Tauth:
        case Tattach:
        case Twalk:
        case Topen:
        case Tcreate:
        case Tread:
        case Twrite:
        case Tclunk:
        case Tremove:
        case Tstat:
        case Twstat:
        case Tflush:
            valid_type = true;
            break;
        default:
            /* Unknown message type - send error and return */
            p9_send_error(client, tag, "unknown message type");
            g_server.stats.messages_received++;
            return;
    }
    
    /* Initialize response message with correct response type */
    uint8_t resp_type = type + 1;  /* Response type = request type + 1 */
    p9_msg_init_write(&resp, client->tx_buffer, P9_MAX_MSG_SIZE, resp_type, tag);
    
    /* p9_msg_init_write reserves space for header - handlers write payload */
    /* Dispatch to appropriate handler */
    switch (type) {
        case Tversion:
            p9_handle_version(client, &req, &resp);
            break;
        case Tauth:
            p9_handle_auth(client, &req, &resp);
            break;
        case Tattach:
            p9_handle_attach(client, &req, &resp);
            break;
        case Twalk:
            p9_handle_walk(client, &req, &resp);
            break;
        case Topen:
            p9_handle_open(client, &req, &resp);
            break;
        case Tcreate:
            p9_handle_create(client, &req, &resp);
            break;
        case Tread:
            p9_handle_read(client, &req, &resp);
            break;
        case Twrite:
            p9_handle_write(client, &req, &resp);
            break;
        case Tclunk:
            p9_handle_clunk(client, &req, &resp);
            break;
        case Tremove:
            p9_handle_remove(client, &req, &resp);
            break;
        case Tstat:
            p9_handle_stat(client, &req, &resp);
            break;
        case Twstat:
            p9_handle_wstat(client, &req, &resp);
            break;
        case Tflush:
            p9_handle_flush(client, &req, &resp);
            break;
    }
    
    /* Finalize response message (fills in size) */
    p9_msg_finalize(&resp);
    
    /* Send response */
    if (client->pcb && resp.pos > 0) {
        err_t err = tcp_write(client->pcb, resp.data, resp.pos, TCP_WRITE_FLAG_COPY);
        if (err == ERR_OK) {
            tcp_output(client->pcb);
            g_server.stats.messages_sent++;
        } else {
            g_server.stats.errors++;
        }
    }
    
    g_server.stats.messages_received++;
}

static void p9_send_error(p9_client_t *client, uint16_t tag, const char *ename) {
    p9_msg_t resp;
    p9_msg_init_write(&resp, client->tx_buffer, P9_MAX_MSG_SIZE, Rerror, tag);
    
    /* p9_msg_init_write already reserved space for header (size, type, tag)
     * Just write the error string - p9_msg_finalize will fill in the header */
    p9_write_string(&resp, ename);
    
    p9_msg_finalize(&resp);
    
    if (client->pcb && resp.pos > 0) {
        tcp_write(client->pcb, resp.data, resp.pos, TCP_WRITE_FLAG_COPY);
        tcp_output(client->pcb);
    }
}

/* ========================================================================
 * FID Management Functions
 * ======================================================================== */

void p9_fid_table_init(p9_fid_table_t *table) {
    memset(table, 0, sizeof(p9_fid_table_t));
    table->next_qid_path = 1;  /* Start at 1, 0 is reserved */
}

p9_fid_t *p9_fid_alloc(p9_fid_table_t *table, uint32_t fid_num) {
    /* Check if FID already exists */
    for (int i = 0; i < P9_MAX_FIDS_PER_CLIENT; i++) {
        if (table->fids[i].in_use && table->fids[i].fid == fid_num) {
            return NULL;  /* FID already in use */
        }
    }
    
    /* Find free slot */
    for (int i = 0; i < P9_MAX_FIDS_PER_CLIENT; i++) {
        if (!table->fids[i].in_use) {
            memset(&table->fids[i], 0, sizeof(p9_fid_t));
            table->fids[i].in_use = true;
            table->fids[i].fid = fid_num;
            return &table->fids[i];
        }
    }
    
    return NULL;  /* No free slots */
}

p9_fid_t *p9_fid_get(p9_fid_table_t *table, uint32_t fid_num) {
    for (int i = 0; i < P9_MAX_FIDS_PER_CLIENT; i++) {
        if (table->fids[i].in_use && table->fids[i].fid == fid_num) {
            return &table->fids[i];
        }
    }
    return NULL;
}

p9_fid_t *p9_fid_clone(p9_fid_table_t *table, uint32_t old_fid, uint32_t new_fid) {
    p9_fid_t *old = p9_fid_get(table, old_fid);
    if (!old) {
        return NULL;
    }
    
    p9_fid_t *new = p9_fid_alloc(table, new_fid);
    if (!new) {
        return NULL;
    }
    
    /* Copy FID data (but not the file handle) */
    new->type = old->type;
    new->qid = old->qid;
    strncpy(new->path, old->path, FAT32_MAX_PATH_LEN - 1);
    new->path[FAT32_MAX_PATH_LEN - 1] = '\0';
    
    return new;
}

void p9_fid_free(p9_fid_table_t *table, uint32_t fid_num) {
    p9_fid_t *fid = p9_fid_get(table, fid_num);
    if (!fid) {
        return;
    }
    
    /* Close file if open */
    if (fid->file.is_open) {
        fat32_sync_close(&fid->file);
    }
    
    /* Mark as free */
    fid->in_use = false;
}

void p9_fid_free_all(p9_fid_table_t *table) {
    for (int i = 0; i < P9_MAX_FIDS_PER_CLIENT; i++) {
        if (table->fids[i].in_use) {
            if (table->fids[i].file.is_open) {
                fat32_sync_close(&table->fids[i].file);
            }
            table->fids[i].in_use = false;
        }
    }
}

uint64_t p9_fid_next_qid_path(p9_fid_table_t *table) {
    return table->next_qid_path++;
}