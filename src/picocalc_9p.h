/**
 * @file picocalc_9p.h
 * @brief 9P2000.u Protocol Server for PicoCalc
 * 
 * Main server infrastructure providing TCP server, client management,
 * FID table, and message dispatch for the 9P filesystem protocol.
 */

#ifndef PICOCALC_9P_H
#define PICOCALC_9P_H

#include <stdint.h>
#include <stdbool.h>
#include "picocalc_9p_proto.h"
#include "picocalc_fat32_sync.h"

/* ========================================================================
 * Configuration Constants
 * ======================================================================== */

/** Default 9P server port (standard is 564) */
#define P9_SERVER_PORT 564

/** Maximum number of concurrent clients */
#define P9_MAX_CLIENTS 3

/** Maximum number of FIDs per client */
#define P9_MAX_FIDS_PER_CLIENT 64

/** Maximum message size (8KB default, can be negotiated) */
#define P9_MAX_MSG_SIZE 8192

/** Server version string */
#define P9_SERVER_VERSION "9P2000.u"

/** mDNS service name */
#define P9_MDNS_SERVICE_NAME "picocalc"

/** mDNS service type */
#define P9_MDNS_SERVICE_TYPE "_9p._tcp"

/* ========================================================================
 * FID Management
 * ======================================================================== */

/** FID types */
typedef enum {
    P9_FID_TYPE_NONE = 0,
    P9_FID_TYPE_FILE,
    P9_FID_TYPE_DIR,
    P9_FID_TYPE_AUTH,  /* Authentication FID (not fully implemented) */
} p9_fid_type_t;

/** FID state */
typedef struct {
    bool in_use;
    p9_fid_type_t type;
    uint32_t fid;
    p9_qid_t qid;
    char path[FAT32_MAX_PATH_LEN];
    fat32_file_t file;
    uint32_t iounit;  /* Optimal I/O unit size */
    uint8_t mode;     /* Open mode flags */
} p9_fid_t;

/** FID table for a client */
typedef struct {
    p9_fid_t fids[P9_MAX_FIDS_PER_CLIENT];
    uint32_t next_qid_path;  /* Counter for generating unique QID paths */
} p9_fid_table_t;

/* ========================================================================
 * Client Connection Management
 * ======================================================================== */

/** Client connection state */
typedef enum {
    P9_CLIENT_STATE_DISCONNECTED = 0,
    P9_CLIENT_STATE_CONNECTED,
    P9_CLIENT_STATE_VERSION_NEGOTIATED,
    P9_CLIENT_STATE_ATTACHED,
    P9_CLIENT_STATE_ERROR,
} p9_client_state_t;

/** Client connection structure */
typedef struct {
    bool active;
    p9_client_state_t state;
    struct tcp_pcb *pcb;
    uint32_t max_msg_size;
    char version[32];
    p9_fid_table_t fid_table;
    uint8_t rx_buffer[P9_MAX_MSG_SIZE];
    uint32_t rx_len;
    uint8_t tx_buffer[P9_MAX_MSG_SIZE];
} p9_client_t;

/* ========================================================================
 * Server State
 * ======================================================================== */

/** Server state */
typedef enum {
    P9_SERVER_STATE_STOPPED = 0,
    P9_SERVER_STATE_STARTING,
    P9_SERVER_STATE_RUNNING,
    P9_SERVER_STATE_STOPPING,
    P9_SERVER_STATE_ERROR,
} p9_server_state_t;

/** Server statistics */
typedef struct {
    uint32_t total_connections;
    uint32_t active_connections;
    uint32_t messages_received;
    uint32_t messages_sent;
    uint32_t errors;
    uint64_t bytes_read;
    uint64_t bytes_written;
} p9_server_stats_t;

/* ========================================================================
 * Public API
 * ======================================================================== */

/**
 * @brief Initialize 9P server
 * 
 * Must be called before any other p9_server_* functions.
 * Should be called from Core 1 initialization.
 * 
 * @return true on success, false on failure
 */
bool p9_server_init(void);

/**
 * @brief Start 9P server
 * 
 * Starts TCP listener on configured port and begins accepting connections.
 * This function should be called after WiFi connection is established.
 * 
 * @return true on success, false on failure
 */
bool p9_server_start(void);

/**
 * @brief Stop 9P server
 * 
 * Gracefully closes all client connections and stops the TCP listener.
 */
void p9_server_stop(void);

/**
 * @brief Process 9P server events
 * 
 * Must be called periodically from Core 1 main loop to process
 * incoming messages and maintain connections.
 */
void p9_server_poll(void);

/**
 * @brief Get server state
 * 
 * @return Current server state
 */
p9_server_state_t p9_server_get_state(void);

/**
 * @brief Get server statistics
 * 
 * @param stats Pointer to statistics structure to populate
 */
void p9_server_get_stats(p9_server_stats_t *stats);

/**
 * @brief Check if server is running
 * 
 * @return true if server is running and accepting connections
 */
bool p9_server_is_running(void);

/**
 * @brief Get number of active clients
 * 
 * @return Number of currently connected clients
 */
uint32_t p9_server_get_client_count(void);

/* ========================================================================
 * FID Management Functions
 * ======================================================================== */

/**
 * @brief Initialize FID table
 * 
 * @param table FID table to initialize
 */
void p9_fid_table_init(p9_fid_table_t *table);

/**
 * @brief Allocate a new FID
 * 
 * @param table FID table
 * @param fid_num FID number to allocate
 * @return Pointer to allocated FID, or NULL if already in use
 */
p9_fid_t *p9_fid_alloc(p9_fid_table_t *table, uint32_t fid_num);

/**
 * @brief Get existing FID
 * 
 * @param table FID table
 * @param fid_num FID number to look up
 * @return Pointer to FID, or NULL if not found
 */
p9_fid_t *p9_fid_get(p9_fid_table_t *table, uint32_t fid_num);

/**
 * @brief Clone a FID
 * 
 * @param table FID table
 * @param old_fid Source FID number
 * @param new_fid Destination FID number
 * @return Pointer to new FID, or NULL on failure
 */
p9_fid_t *p9_fid_clone(p9_fid_table_t *table, uint32_t old_fid, uint32_t new_fid);

/**
 * @brief Free a FID
 * 
 * @param table FID table
 * @param fid_num FID number to free
 */
void p9_fid_free(p9_fid_table_t *table, uint32_t fid_num);

/**
 * @brief Free all FIDs in table
 * 
 * @param table FID table
 */
void p9_fid_free_all(p9_fid_table_t *table);

/**
 * @brief Generate next unique QID path
 * 
 * @param table FID table
 * @return Unique QID path value
 */
uint64_t p9_fid_next_qid_path(p9_fid_table_t *table);

/* ========================================================================
 * Message Handler Functions (implemented in picocalc_9p_handlers.c)
 * ======================================================================== */

/**
 * @brief Handle Tversion message
 */
void p9_handle_version(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp);

/**
 * @brief Handle Tattach message
 */
void p9_handle_attach(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp);

/**
 * @brief Handle Twalk message
 */
void p9_handle_walk(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp);

/**
 * @brief Handle Topen message
 */
void p9_handle_open(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp);

/**
 * @brief Handle Tcreate message
 */
void p9_handle_create(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp);

/**
 * @brief Handle Tread message
 */
void p9_handle_read(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp);

/**
 * @brief Handle Twrite message
 */
void p9_handle_write(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp);

/**
 * @brief Handle Tclunk message
 */
void p9_handle_clunk(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp);

/**
 * @brief Handle Tremove message
 */
void p9_handle_remove(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp);

/**
 * @brief Handle Tstat message
 */
void p9_handle_stat(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp);

/**
 * @brief Handle Twstat message
 */
void p9_handle_wstat(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp);

/**
 * @brief Handle Tauth message (stub)
 */
void p9_handle_auth(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp);

/**
 * @brief Handle Tflush message
 */
void p9_handle_flush(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp);

/* ========================================================================
 * Core 1 Management Functions (implemented in picocalc_9p_core1.c)
 * ======================================================================== */

/**
 * @brief Launch Core 1 with 9P server
 *
 * Should be called during system initialization (from Core 0).
 * Starts Core 1 and initializes the 9P server infrastructure.
 */
void p9_core1_launch(void);

/**
 * @brief Request 9P server to start
 *
 * Called from Core 0 when WiFi connection is established.
 * Signals Core 1 to start the 9P server and mDNS responder.
 */
void p9_server_request_start(void);

/**
 * @brief Request 9P server to stop
 *
 * Called from Core 0 when WiFi connection is lost.
 * Signals Core 1 to stop the 9P server and mDNS responder.
 */
void p9_server_request_stop(void);

/**
 * @brief Check if 9P server is active
 *
 * @return true if server is running and accepting connections
 */
bool p9_server_is_active(void);

/**
 * @brief Get server statistics
 *
 * @param stats Pointer to statistics structure to populate
 */
void p9_server_get_statistics(p9_server_stats_t *stats);

#endif /* PICOCALC_9P_H */