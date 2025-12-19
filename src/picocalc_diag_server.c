/**
 * @file picocalc_diag_server.c
 * @brief Simple NEX Diagnostic Server
 *
 * Provides a simple NEX server on port 1900 that returns system status
 * including WiFi info and connection statistics.
 * Used for debugging incoming connection issues.
 * 
 * NEX Protocol: Client sends path (e.g., "/status\r\n"), server responds with text.
 */

#include "picocalc_diag_server.h"
#include "picocalc_debug_log.h"
#include "pico/stdlib.h"
#include "lwip/tcp.h"
#include "lwip/err.h"
#include "picocalc_wifi.h"
#include "build_version.h"
#include <string.h>
#include <stdio.h>

#define DIAG_PORT 1901
#define DIAG_MAX_CLIENTS 2

typedef struct {
    struct tcp_pcb *pcb;
    bool active;
    char rx_buffer[256];
    uint16_t rx_len;
    uint32_t request_count;
} diag_client_t;

static struct {
    struct tcp_pcb *listen_pcb;
    diag_client_t clients[DIAG_MAX_CLIENTS];
    bool running;
    uint32_t total_requests;
    uint32_t total_connections;
} g_diag_server;

/* Forward declarations */
static err_t diag_accept(void *arg, struct tcp_pcb *newpcb, err_t err);
static err_t diag_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static void diag_err(void *arg, err_t err);
static void diag_close_client(diag_client_t *client);
static void diag_send_status(diag_client_t *client);

bool diag_server_init(void) {
    memset(&g_diag_server, 0, sizeof(g_diag_server));
    return true;
}

bool diag_server_start(void) {
    if (g_diag_server.running) {
        return false;
    }
    
    /* Create TCP listening socket */
    g_diag_server.listen_pcb = tcp_new();
    if (!g_diag_server.listen_pcb) {
        return false;
    }
    
    /* Bind to NEX port 1900 */
    err_t err = tcp_bind(g_diag_server.listen_pcb, IP_ADDR_ANY, DIAG_PORT);
    if (err != ERR_OK) {
        tcp_close(g_diag_server.listen_pcb);
        g_diag_server.listen_pcb = NULL;
        return false;
    }
    
    /* Start listening */
    g_diag_server.listen_pcb = tcp_listen(g_diag_server.listen_pcb);
    if (!g_diag_server.listen_pcb) {
        return false;
    }
    
    /* Set accept callback */
    tcp_accept(g_diag_server.listen_pcb, diag_accept);
    
    g_diag_server.running = true;
    return true;
}

void diag_server_stop(void) {
    if (!g_diag_server.running) {
        return;
    }
    
    /* Close all clients */
    for (int i = 0; i < DIAG_MAX_CLIENTS; i++) {
        if (g_diag_server.clients[i].active) {
            diag_close_client(&g_diag_server.clients[i]);
        }
    }
    
    /* Close listening socket */
    if (g_diag_server.listen_pcb) {
        tcp_close(g_diag_server.listen_pcb);
        g_diag_server.listen_pcb = NULL;
    }
    
    g_diag_server.running = false;
}

bool diag_server_is_running(void) {
    return g_diag_server.running;
}

static err_t diag_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    if (err != ERR_OK || !newpcb) {
        return ERR_VAL;
    }
    
    /* Find free client slot */
    diag_client_t *client = NULL;
    for (int i = 0; i < DIAG_MAX_CLIENTS; i++) {
        if (!g_diag_server.clients[i].active) {
            client = &g_diag_server.clients[i];
            break;
        }
    }
    
    if (!client) {
        tcp_close(newpcb);
        return ERR_MEM;
    }
    
    /* Initialize client */
    memset(client, 0, sizeof(diag_client_t));
    client->active = true;
    client->pcb = newpcb;
    
    g_diag_server.total_connections++;
    
    /* Set TCP callbacks */
    tcp_arg(newpcb, client);
    tcp_recv(newpcb, diag_recv);
    tcp_err(newpcb, diag_err);
    
    return ERR_OK;
}

static err_t diag_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    diag_client_t *client = (diag_client_t *)arg;
    
    if (!p) {
        /* Connection closed */
        diag_close_client(client);
        return ERR_OK;
    }
    
    if (err != ERR_OK) {
        pbuf_free(p);
        diag_close_client(client);
        return err;
    }
    
    /* Copy data to buffer */
    uint16_t copy_len = p->tot_len;
    if (client->rx_len + copy_len > sizeof(client->rx_buffer) - 1) {
        copy_len = sizeof(client->rx_buffer) - 1 - client->rx_len;
    }
    
    pbuf_copy_partial(p, client->rx_buffer + client->rx_len, copy_len, 0);
    client->rx_len += copy_len;
    client->rx_buffer[client->rx_len] = '\0';
    
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    
    /* Check if we have a complete request (ends with \r\n or \n) */
    if (strchr(client->rx_buffer, '\n')) {
        client->request_count++;
        g_diag_server.total_requests++;
        
        /* Send status response */
        diag_send_status(client);
        
        /* Close connection after response */
        diag_close_client(client);
    }
    
    return ERR_OK;
}

static void diag_send_status(diag_client_t *client) {
    static char response[4096];  /* Static to avoid stack overflow */
    int len = 0;
    
    /* NEX response format: plain text */
    len += snprintf(response + len, sizeof(response) - len,
        "# PicoCalc Diagnostic Server\n\n");
    
    len += snprintf(response + len, sizeof(response) - len,
        "Firmware: v%s build %d\n\n",
        BUILD_VERSION, BUILD_NUMBER);
    
    /* WiFi status */
    const char *wifi_status = wifi_get_status_string();
    const char *wifi_ip = wifi_get_ip_string();
    len += snprintf(response + len, sizeof(response) - len,
        "## WiFi Status\n"
        "Status: %s\n"
        "IP Address: %s\n\n",
        wifi_status, wifi_ip);
    
    /* Diagnostic server stats */
    len += snprintf(response + len, sizeof(response) - len,
        "## Diagnostic Server Stats\n"
        "Port: %d\n"
        "Total Connections: %lu\n"
        "Total Requests: %lu\n"
        "This Connection: %lu requests\n\n",
        DIAG_PORT,
        (unsigned long)g_diag_server.total_connections,
        (unsigned long)g_diag_server.total_requests,
        (unsigned long)client->request_count);
    
    len += snprintf(response + len, sizeof(response) - len,
        "## Test Commands\n"
        "=> nex://%s/status  Test NEX server\n"
        "$ nc -zv %s 1900   Test NEX port\n"
        "$ curl http://%s:1900  Test with curl\n\n",
        wifi_ip,
        wifi_ip,
        wifi_ip);
    
    /* Add debug log - limit to 2KB to avoid buffer overflow */
    uint32_t log_len = 0;
    const char *log = debug_log_get(&log_len);
    if (log_len > 0) {
        len += snprintf(response + len, sizeof(response) - len,
            "## Debug Log (last %lu bytes)\n", (unsigned long)log_len);
        
        /* Copy log data, ensuring we don't overflow response buffer */
        uint32_t copy_len = log_len;
        uint32_t max_log = 2048;  /* Limit debug log to 2KB in response */
        if (copy_len > max_log) {
            copy_len = max_log;
        }
        if (len + copy_len > sizeof(response) - 100) {  /* Leave 100 bytes margin */
            copy_len = sizeof(response) - len - 100;
        }
        if (copy_len > 0) {
            memcpy(response + len, log, copy_len);
            len += copy_len;
            if (log_len > copy_len) {
                len += snprintf(response + len, sizeof(response) - len,
                    "\n... (truncated, %lu more bytes)\n", (unsigned long)(log_len - copy_len));
            }
        }
        response[len] = '\0';
    } else {
        len += snprintf(response + len, sizeof(response) - len,
            "## Debug Log\n(empty - no debug messages yet)\n\n");
    }
    
    /* Send response */
    if (client->pcb) {
        err_t write_err = tcp_write(client->pcb, response, len, TCP_WRITE_FLAG_COPY);
        if (write_err == ERR_OK) {
            tcp_output(client->pcb);
        }
    }
}

static void diag_err(void *arg, err_t err) {
    diag_client_t *client = (diag_client_t *)arg;
    if (client) {
        client->pcb = NULL;  /* PCB already freed by lwIP */
        diag_close_client(client);
    }
}

static void diag_close_client(diag_client_t *client) {
    if (!client->active) {
        return;
    }
    
    if (client->pcb) {
        tcp_arg(client->pcb, NULL);
        tcp_recv(client->pcb, NULL);
        tcp_err(client->pcb, NULL);
        tcp_close(client->pcb);
        client->pcb = NULL;
    }
    
    client->active = false;
}