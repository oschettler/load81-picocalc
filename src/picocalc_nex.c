#include "picocalc_nex.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include <lua.h>
#include <lauxlib.h>
#include <string.h>
#include <stdlib.h>
#include "debug.h"

#define NEX_PORT 1900
#define NEX_TIMEOUT_MS 10000
#define NEX_BUFFER_SIZE 65536

/* NEX connection state */
typedef struct {
    struct tcp_pcb *pcb;
    char *response_buffer;
    size_t response_len;
    size_t response_capacity;
    bool connected;
    bool complete;
    err_t error;
} nex_connection_t;

/* Initialize NEX */
void nex_init(void) {
    /* NEX protocol initialization */
    DEBUG_PRINTF("[NEX] Protocol support initialized\n");
}

/* TCP connection callback */
static err_t nex_connected_callback(void *arg, struct tcp_pcb *tpcb, err_t err) {
    nex_connection_t *conn = (nex_connection_t *)arg;
    
    if (err != ERR_OK) {
        DEBUG_PRINTF("[NEX] Connection failed: %d\n", err);
        conn->error = err;
        conn->complete = true;
        return err;
    }
    
    DEBUG_PRINTF("[NEX] TCP connected\n");
    conn->connected = true;
    return ERR_OK;
}

/* TCP data received callback */
static err_t nex_recv_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    nex_connection_t *conn = (nex_connection_t *)arg;
    
    if (err != ERR_OK || p == NULL) {
        /* Connection closed or error */
        conn->complete = true;
        if (p) pbuf_free(p);
        return ERR_OK;
    }
    
    /* Append data to response buffer */
    size_t new_len = conn->response_len + p->tot_len;
    if (new_len > conn->response_capacity) {
        size_t new_capacity = new_len + 4096;
        char *new_buffer = realloc(conn->response_buffer, new_capacity);
        if (!new_buffer) {
            DEBUG_PRINTF("[NEX] Out of memory\n");
            conn->error = ERR_MEM;
            conn->complete = true;
            pbuf_free(p);
            return ERR_MEM;
        }
        conn->response_buffer = new_buffer;
        conn->response_capacity = new_capacity;
    }
    
    /* Copy pbuf data to response buffer */
    pbuf_copy_partial(p, conn->response_buffer + conn->response_len, p->tot_len, 0);
    conn->response_len += p->tot_len;
    
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    
    return ERR_OK;
}

/* TCP error callback */
static void nex_error_callback(void *arg, err_t err) {
    nex_connection_t *conn = (nex_connection_t *)arg;
    DEBUG_PRINTF("[NEX] TCP error: %d\n", err);
    conn->error = err;
    conn->complete = true;
}

/* DNS resolution callback */
static void nex_dns_callback(const char *name, const ip_addr_t *ipaddr, void *arg) {
    nex_connection_t *conn = (nex_connection_t *)arg;
    
    if (ipaddr == NULL) {
        DEBUG_PRINTF("[NEX] DNS resolution failed\n");
        conn->error = ERR_ARG;
        conn->complete = true;
        return;
    }
    
    DEBUG_PRINTF("[NEX] Resolved %s to %s\n", name, ipaddr_ntoa(ipaddr));
    
    /* Create TCP connection */
    conn->pcb = tcp_new();
    if (!conn->pcb) {
        DEBUG_PRINTF("[NEX] Failed to create TCP PCB\n");
        conn->error = ERR_MEM;
        conn->complete = true;
        return;
    }
    
    tcp_arg(conn->pcb, conn);
    tcp_recv(conn->pcb, nex_recv_callback);
    tcp_err(conn->pcb, nex_error_callback);
    
    err_t err = tcp_connect(conn->pcb, ipaddr, NEX_PORT, nex_connected_callback);
    if (err != ERR_OK) {
        DEBUG_PRINTF("[NEX] TCP connect failed: %d\n", err);
        conn->error = err;
        conn->complete = true;
        tcp_close(conn->pcb);
        conn->pcb = NULL;
    }
}

/* Lua: nex.load(url) */
static int lua_nex_load(lua_State *L) {
    const char *url = luaL_checkstring(L, 1);
    
    /* Parse URL: nex://hostname/path */
    if (strncmp(url, "nex://", 6) != 0) {
        lua_pushnil(L);
        lua_pushstring(L, "Invalid NEX URL (must start with nex://)");
        return 2;
    }
    
    const char *host_start = url + 6;
    const char *path_start = strchr(host_start, '/');
    
    char hostname[256];
    char path[256];
    
    if (path_start) {
        size_t host_len = path_start - host_start;
        if (host_len >= sizeof(hostname)) host_len = sizeof(hostname) - 1;
        strncpy(hostname, host_start, host_len);
        hostname[host_len] = '\0';
        strncpy(path, path_start, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
    } else {
        strncpy(hostname, host_start, sizeof(hostname) - 1);
        hostname[sizeof(hostname) - 1] = '\0';
        strcpy(path, "/");
    }
    
    DEBUG_PRINTF("[NEX] Loading nex://%s%s\n", hostname, path);
    
    /* Initialize connection state */
    nex_connection_t conn = {0};
    conn.response_buffer = malloc(4096);
    if (!conn.response_buffer) {
        lua_pushnil(L);
        lua_pushstring(L, "Out of memory");
        return 2;
    }
    conn.response_capacity = 4096;
    
    /* Resolve hostname */
    ip_addr_t resolved_addr;
    err_t dns_err = dns_gethostbyname(hostname, &resolved_addr, nex_dns_callback, &conn);
    
    if (dns_err == ERR_OK) {
        /* Already cached */
        nex_dns_callback(hostname, &resolved_addr, &conn);
    } else if (dns_err != ERR_INPROGRESS) {
        DEBUG_PRINTF("[NEX] DNS lookup failed: %d\n", dns_err);
        free(conn.response_buffer);
        lua_pushnil(L);
        lua_pushstring(L, "DNS lookup failed");
        return 2;
    }
    
    /* Wait for DNS and connection */
    absolute_time_t start_time = get_absolute_time();
    while (!conn.complete && !conn.connected) {
        cyw43_arch_poll();
        sleep_ms(10);
        if (absolute_time_diff_us(start_time, get_absolute_time()) > NEX_TIMEOUT_MS * 1000) {
            DEBUG_PRINTF("[NEX] Connection timeout\n");
            if (conn.pcb) {
                tcp_close(conn.pcb);
            }
            free(conn.response_buffer);
            lua_pushnil(L);
            lua_pushstring(L, "Connection timeout");
            return 2;
        }
    }
    
    if (conn.error != ERR_OK) {
        if (conn.pcb) tcp_close(conn.pcb);
        free(conn.response_buffer);
        lua_pushnil(L);
        lua_pushstring(L, "Connection error");
        return 2;
    }
    
    /* Send NEX request */
    char request[512];
    snprintf(request, sizeof(request), "nex://%s%s\r\n", hostname, path);
    
    err_t write_err = tcp_write(conn.pcb, request, strlen(request), TCP_WRITE_FLAG_COPY);
    if (write_err != ERR_OK) {
        DEBUG_PRINTF("[NEX] Failed to send request: %d\n", write_err);
        tcp_close(conn.pcb);
        free(conn.response_buffer);
        lua_pushnil(L);
        lua_pushstring(L, "Failed to send request");
        return 2;
    }
    
    tcp_output(conn.pcb);
    DEBUG_PRINTF("[NEX] Request sent, waiting for response...\n");
    
    /* Wait for response */
    conn.complete = false;
    start_time = get_absolute_time();
    while (!conn.complete) {
        cyw43_arch_poll();
        sleep_ms(10);
        if (absolute_time_diff_us(start_time, get_absolute_time()) > NEX_TIMEOUT_MS * 1000) {
            DEBUG_PRINTF("[NEX] Response timeout\n");
            tcp_close(conn.pcb);
            free(conn.response_buffer);
            lua_pushnil(L);
            lua_pushstring(L, "Response timeout");
            return 2;
        }
    }
    
    tcp_close(conn.pcb);
    
    /* Return response */
    if (conn.response_len > 0) {
        conn.response_buffer[conn.response_len] = '\0';
        DEBUG_PRINTF("[NEX] Received %zu bytes\n", conn.response_len);
        lua_pushlstring(L, conn.response_buffer, conn.response_len);
        free(conn.response_buffer);
        return 1;
    } else {
        free(conn.response_buffer);
        lua_pushnil(L);
        lua_pushstring(L, "Empty response");
        return 2;
    }
}

/* Lua: nex.parse(content) */
static int lua_nex_parse(lua_State *L) {
    size_t len;
    const char *content = lua_tolstring(L, 1, &len);
    if (!content) {
        lua_newtable(L);
        return 1;
    }
    
    /* Parse NEX/Gemtext format */
    /* Return structured table */
    lua_newtable(L);
    
    /* Simple line-by-line parsing */
    char *dup = strdup(content);
    char *line = strtok(dup, "\n");
    int index = 1;
    
    while (line) {
        lua_newtable(L);
        
        /* Check line type */
        if (strncmp(line, "=>", 2) == 0) {
            /* Link line */
            lua_pushstring(L, "link");
            lua_setfield(L, -2, "type");
            lua_pushstring(L, line + 2);
            lua_setfield(L, -2, "text");
        } else if (strncmp(line, "#", 1) == 0) {
            /* Heading */
            lua_pushstring(L, "heading");
            lua_setfield(L, -2, "type");
            lua_pushstring(L, line + 1);
            lua_setfield(L, -2, "text");
        } else {
            /* Text line */
            lua_pushstring(L, "text");
            lua_setfield(L, -2, "type");
            lua_pushstring(L, line);
            lua_setfield(L, -2, "text");
        }
        
        lua_rawseti(L, -2, index++);
        line = strtok(NULL, "\n");
    }
    
    free(dup);
    return 1;
}

/* Register NEX Lua API */
void nex_register_lua(lua_State *L) {
    /* Create nex table */
    lua_newtable(L);
    
    lua_pushcfunction(L, lua_nex_load);
    lua_setfield(L, -2, "load");
    
    lua_pushcfunction(L, lua_nex_parse);
    lua_setfield(L, -2, "parse");
    
    lua_setglobal(L, "nex");
}
