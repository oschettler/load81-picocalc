# 9P2000.u Protocol Server Architecture for PicoCalc

## Overview

This document describes the architecture for implementing a 9P2000.u protocol server on the PicoCalc firmware, enabling Linux systems to mount the SD card filesystem over TCP/IP using the standard v9fs client.

## System Architecture

### High-Level Design

```
┌─────────────────────────────────────────────────────────────┐
│                    PicoCalc RP2350                          │
│                                                             │
│  ┌──────────────────────┐    ┌─────────────────────────┐  │
│  │      Core 0          │    │       Core 1            │  │
│  │  ┌────────────────┐  │    │  ┌──────────────────┐  │  │
│  │  │ Lua Interpreter│  │    │  │  9P Server       │  │  │
│  │  │ Graphics       │  │    │  │  - TCP Listener  │  │  │
│  │  │ UI/Menu        │  │    │  │  - Protocol      │  │  │
│  │  │ WiFi Manager   │  │    │  │  - Client Mgmt   │  │  │
│  │  └────────────────┘  │    │  │  - mDNS Responder│  │  │
│  │         │             │    │  └──────────────────┘  │  │
│  └─────────┼─────────────┘    └───────────┼────────────┘  │
│            │                               │                │
│            └───────────┬───────────────────┘                │
│                        │                                    │
│                 ┌──────▼──────┐                            │
│                 │   Mutex     │                            │
│                 │  Protected  │                            │
│                 │  FAT32 API  │                            │
│                 └──────┬──────┘                            │
│                        │                                    │
│                 ┌──────▼──────┐                            │
│                 │  SD Card    │                            │
│                 │  (SPI)      │                            │
│                 └─────────────┘                            │
└─────────────────────────────────────────────────────────────┘
                         │
                         │ Network (WiFi)
                         │
                ┌────────▼────────┐
                │  Linux Client   │
                │  mount -t 9p    │
                └─────────────────┘
```

## Component Design

### 1. 9P Protocol Implementation

#### 1.1 Protocol Version
- **Target**: 9P2000.u (Unix extensions)
- **Features**: 
  - Basic file operations (open, read, write, close)
  - Directory operations (readdir, mkdir, remove)
  - Metadata operations (stat, wstat)
  - Unix extensions (uid/gid support - stubbed)
  - No authentication (network security model)

#### 1.2 Message Structure

```c
typedef struct {
    uint32_t size;      // Total message size including header
    uint8_t type;       // Message type (Tversion, Tattach, etc.)
    uint16_t tag;       // Client-assigned tag for matching responses
    uint8_t data[];     // Variable-length message data
} p9_msg_t;
```

#### 1.3 Core Message Types

**Version Negotiation:**
- `Tversion` / `Rversion` - Protocol version and max message size

**Authentication (Stubbed):**
- `Tauth` / `Rauth` - Authentication (returns error, no auth required)

**Connection:**
- `Tattach` / `Rattach` - Attach to filesystem root

**File Operations:**
- `Twalk` / `Rwalk` - Navigate directory tree
- `Topen` / `Ropen` - Open file/directory
- `Tcreate` / `Rcreate` - Create new file/directory
- `Tread` / `Rread` - Read file data
- `Twrite` / `Rwrite` - Write file data
- `Tclunk` / `Rclunk` - Close file handle
- `Tremove` / `Rremove` - Delete file/directory
- `Tstat` / `Rstat` - Get file metadata
- `Twstat` / `Rwstat` - Set file metadata

#### 1.4 QID (Unique File Identifier)

```c
typedef struct {
    uint8_t type;       // File type (dir, file, etc.)
    uint32_t version;   // Version number (for cache coherency)
    uint64_t path;      // Unique file identifier
} p9_qid_t;
```

**QID Generation Strategy:**
- Use FAT32 cluster number as base path
- Add directory entry offset for uniqueness
- Type derived from FAT32 attributes
- Version incremented on modification

### 2. Server Architecture

#### 2.1 Core Components

```c
// Main server state
typedef struct {
    bool running;
    struct tcp_pcb *listen_pcb;
    p9_client_t *clients[MAX_CLIENTS];  // 3 concurrent clients
    mutex_t fat32_mutex;
    uint32_t next_fid;
} p9_server_t;

// Per-client state
typedef struct {
    struct tcp_pcb *pcb;
    uint16_t client_id;
    uint8_t rx_buffer[P9_MAX_MSG_SIZE];
    uint32_t rx_len;
    p9_fid_table_t *fids;  // File ID table
    bool authenticated;     // Always true (no auth)
} p9_client_t;

// File ID (FID) tracking
typedef struct {
    uint32_t fid;
    fat32_file_t file;
    p9_qid_t qid;
    bool is_open;
    char path[FAT32_MAX_PATH_LEN];
} p9_fid_entry_t;
```

#### 2.2 Message Processing Flow

```
TCP Data Received
    │
    ▼
Parse 9P Message Header
    │
    ▼
Validate Message
    │
    ▼
Acquire FAT32 Mutex
    │
    ▼
Dispatch to Handler
    │
    ├─► Tversion → Negotiate protocol
    ├─► Tattach → Attach to root
    ├─► Twalk → Navigate path
    ├─► Topen → Open file
    ├─► Tread → Read data
    ├─► Twrite → Write data
    ├─► Tstat → Get metadata
    ├─► Tclunk → Close file
    └─► Tremove → Delete file
    │
    ▼
Release FAT32 Mutex
    │
    ▼
Build Response Message
    │
    ▼
Send via TCP
```

### 3. Thread-Safe FAT32 Access

#### 3.1 Mutex Protection Layer

```c
// Wrapper functions with mutex protection
typedef struct {
    mutex_t lock;
    bool initialized;
} fat32_sync_t;

fat32_error_t fat32_sync_open(fat32_file_t *file, const char *path);
fat32_error_t fat32_sync_read(fat32_file_t *file, void *buf, size_t size, size_t *read);
fat32_error_t fat32_sync_write(fat32_file_t *file, const void *buf, size_t size, size_t *written);
fat32_error_t fat32_sync_close(fat32_file_t *file);
// ... etc for all FAT32 operations
```

#### 3.2 Synchronization Strategy

1. **Mutex Initialization**: Create mutex in `fat32_init()`
2. **Lock Acquisition**: Before any FAT32 operation
3. **Lock Release**: After operation completes
4. **Timeout Handling**: Use `mutex_enter_timeout_ms()` with 5s timeout
5. **Error Recovery**: Release mutex on error paths

### 4. TCP Server Implementation

#### 4.1 Server Lifecycle

```c
// Initialize on Core 1
void p9_server_init(void) {
    // Create listening socket on port 564
    // Set up lwIP callbacks
    // Initialize client pool
    // Initialize FAT32 mutex
}

// Main server loop (runs on Core 1)
void p9_server_run(void) {
    while (server.running) {
        cyw43_arch_poll();  // Process network events
        p9_process_clients();  // Handle client messages
        tight_loop_contents();
    }
}
```

#### 4.2 Connection Management

```c
// Accept new client connection
static err_t p9_accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    if (num_clients >= MAX_CLIENTS) {
        tcp_abort(newpcb);
        return ERR_ABRT;
    }
    
    p9_client_t *client = allocate_client();
    client->pcb = newpcb;
    tcp_arg(newpcb, client);
    tcp_recv(newpcb, p9_recv_callback);
    tcp_err(newpcb, p9_err_callback);
    
    return ERR_OK;
}

// Receive data from client
static err_t p9_recv_callback(void *arg, struct tcp_pcb *tpcb, 
                              struct pbuf *p, err_t err) {
    p9_client_t *client = (p9_client_t *)arg;
    
    if (p == NULL) {
        // Connection closed
        p9_close_client(client);
        return ERR_OK;
    }
    
    // Copy data to client buffer
    pbuf_copy_partial(p, client->rx_buffer + client->rx_len, 
                      p->tot_len, 0);
    client->rx_len += p->tot_len;
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    
    // Process complete messages
    p9_process_messages(client);
    
    return ERR_OK;
}
```

### 5. mDNS Service Discovery

#### 5.1 mDNS Responder

```c
// Advertise 9P service
typedef struct {
    char hostname[32];      // "picocalc"
    char service[32];       // "_9p._tcp"
    uint16_t port;          // 564
    char txt_records[128];  // "version=9P2000.u"
} mdns_service_t;

void mdns_init(void) {
    // Register hostname: picocalc.local
    // Register service: _9p._tcp.local
    // Set TXT records with protocol version
}

void mdns_respond(void) {
    // Listen for mDNS queries on 224.0.0.251:5353
    // Respond to queries for picocalc.local
    // Respond to service discovery queries
}
```

#### 5.2 Service Advertisement

- **Hostname**: `picocalc.local`
- **Service Type**: `_9p._tcp.local`
- **Port**: 564
- **TXT Records**:
  - `version=9P2000.u`
  - `export=/`
  - `auth=none`

### 6. Core 1 Integration

#### 6.1 Initialization Sequence

```c
// In main.c - after WiFi initialization
void core1_entry(void) {
    // Wait for WiFi connection
    while (!wifi_is_connected()) {
        sleep_ms(100);
    }
    
    // Initialize 9P server
    p9_server_init();
    
    // Initialize mDNS
    mdns_init();
    
    // Run server loop
    p9_server_run();
}

// Launch Core 1
void start_9p_server(void) {
    multicore_launch_core1(core1_entry);
}
```

#### 6.2 WiFi Connection Callback

```c
// In picocalc_wifi.c
void wifi_on_connected(void) {
    // Start 9P server on Core 1
    if (!p9_server_is_running()) {
        start_9p_server();
    }
}
```

### 7. Memory Management

#### 7.1 Buffer Allocation

```c
// Pre-allocated buffer pool
#define P9_MAX_MSG_SIZE 8192
#define P9_BUFFER_POOL_SIZE 6  // 2 per client

typedef struct {
    uint8_t buffers[P9_BUFFER_POOL_SIZE][P9_MAX_MSG_SIZE];
    bool in_use[P9_BUFFER_POOL_SIZE];
    mutex_t lock;
} p9_buffer_pool_t;

uint8_t* p9_alloc_buffer(void);
void p9_free_buffer(uint8_t *buf);
```

#### 7.2 Memory Constraints

- **Total RAM**: ~520KB (RP2350)
- **9P Server Budget**: ~64KB
  - Message buffers: 48KB (6 × 8KB)
  - Client state: 6KB (3 clients)
  - FID tables: 6KB
  - Stack/misc: 4KB

### 8. Error Handling

#### 8.1 Error Response Generation

```c
void p9_send_error(p9_client_t *client, uint16_t tag, const char *ename) {
    p9_msg_t *resp = p9_alloc_buffer();
    resp->type = Rerror;
    resp->tag = tag;
    // Encode error string
    p9_send_message(client, resp);
    p9_free_buffer(resp);
}
```

#### 8.2 Common Error Conditions

- `ENOENT` - File not found
- `EACCES` - Permission denied (rare, mostly read-only checks)
- `EEXIST` - File already exists
- `ENOSPC` - Disk full
- `EIO` - SD card I/O error
- `ENOMEM` - Out of memory
- `EINVAL` - Invalid parameter

### 9. File Operations Mapping

#### 9.1 FAT32 to 9P Mapping

| 9P Operation | FAT32 Function | Notes |
|--------------|----------------|-------|
| Twalk | `fat32_open()` | Navigate path components |
| Topen | `fat32_open()` | Open existing file |
| Tcreate | `fat32_create()` | Create new file |
| Tread | `fat32_read()` | Read file data |
| Twrite | `fat32_write()` | Write file data |
| Tstat | FAT32 entry | Get file attributes |
| Twstat | FAT32 entry | Set file attributes (limited) |
| Tclunk | `fat32_close()` | Close file handle |
| Tremove | `fat32_delete()` | Delete file |

#### 9.2 Directory Operations

```c
// Readdir implementation
p9_error_t p9_handle_read_dir(p9_client_t *client, p9_fid_entry_t *fid) {
    fat32_entry_t entry;
    while (fat32_dir_read(&fid->file, &entry) == FAT32_OK) {
        // Convert FAT32 entry to 9P stat structure
        p9_stat_t stat;
        fat32_to_p9_stat(&entry, &stat);
        // Append to response buffer
        p9_append_stat(response, &stat);
    }
}
```

### 10. Configuration

#### 10.1 Compile-Time Options

```c
// In picocalc_9p.h
#define P9_SERVER_PORT 564
#define P9_MAX_CLIENTS 3
#define P9_MAX_MSG_SIZE 8192
#define P9_MAX_FIDS_PER_CLIENT 32
#define P9_PROTOCOL_VERSION "9P2000.u"
#define P9_ENABLE_MDNS 1
#define P9_MDNS_HOSTNAME "picocalc"
```

#### 10.2 Runtime Configuration

- Auto-start on WiFi connection
- Configurable via `/load81/9p.conf` file:
  ```
  port=564
  max_clients=3
  hostname=picocalc
  export=/
  ```

## Implementation Files

### New Files to Create

1. **src/picocalc_9p.h** - Main 9P server header
2. **src/picocalc_9p.c** - 9P server implementation
3. **src/picocalc_9p_proto.h** - Protocol definitions
4. **src/picocalc_9p_proto.c** - Protocol message handling
5. **src/picocalc_9p_fs.c** - Filesystem operations
6. **src/picocalc_mdns.h** - mDNS responder header
7. **src/picocalc_mdns.c** - mDNS responder implementation
8. **src/picocalc_fat32_sync.h** - Thread-safe FAT32 wrapper
9. **src/picocalc_fat32_sync.c** - Mutex-protected FAT32 operations

### Modified Files

1. **CMakeLists.txt** - Add new source files
2. **src/main.c** - Launch Core 1 with 9P server
3. **src/picocalc_wifi.c** - Add connection callback
4. **src/lwipopts.h** - Adjust memory settings if needed

## Testing Strategy

### Unit Tests

1. Protocol message parsing/serialization
2. QID generation and tracking
3. FID table management
4. Buffer pool allocation

### Integration Tests

1. Single client connection and basic operations
2. Multiple concurrent clients
3. Large file transfers
4. Directory traversal
5. Error handling and recovery

### System Tests

1. Mount from Linux: `mount -t 9p -o trans=tcp,port=564 <ip> /mnt`
2. File operations: create, read, write, delete
3. Directory operations: mkdir, rmdir, ls
4. Concurrent access from multiple clients
5. SD card removal/insertion handling
6. WiFi disconnect/reconnect handling

## Performance Considerations

### Optimization Targets

1. **Latency**: < 10ms for small operations
2. **Throughput**: > 500 KB/s for sequential reads/writes
3. **Memory**: < 64KB total footprint
4. **CPU**: < 50% Core 1 utilization under load

### Bottlenecks

1. SD card SPI speed (limited by hardware)
2. FAT32 cluster chain traversal
3. Network packet processing
4. Mutex contention between cores

## Security Considerations

### Network Security

- No authentication (relies on network isolation)
- No encryption (plain TCP)
- Suitable for trusted local networks only
- Consider adding WPA2 requirement

### Filesystem Security

- Read/write access to entire SD card
- No user/permission enforcement
- FAT32 has no native permissions
- Consider read-only mode option

## Future Enhancements

1. **9P2000.L Support** - Linux-specific extensions
2. **Authentication** - Add basic auth framework
3. **TLS Support** - Encrypted connections
4. **Performance Tuning** - Optimize buffer sizes
5. **Caching** - Add directory entry cache
6. **Statistics** - Track operations and performance
7. **Configuration UI** - Menu-based server config

## References

- [9P2000 Protocol Specification](http://man.cat-v.org/plan_9/5/intro)
- [9P2000.u Extensions](http://ericvh.github.io/9p-rfc/rfc9p2000.u.html)
- [Linux v9fs Documentation](https://www.kernel.org/doc/Documentation/filesystems/9p.txt)
- [Plan 9 from User Space](https://9fans.github.io/plan9port/)