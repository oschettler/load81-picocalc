# 9P2000.u Server Implementation Guide

## Implementation Phases

This guide breaks down the implementation into manageable phases, each building on the previous one.

---

## Phase 1: Protocol Foundation

### 1.1 Protocol Data Types (picocalc_9p_proto.h)

```c
#ifndef PICOCALC_9P_PROTO_H
#define PICOCALC_9P_PROTO_H

#include <stdint.h>
#include <stdbool.h>

// Protocol constants
#define P9_VERSION "9P2000.u"
#define P9_NOTAG ((uint16_t)~0)
#define P9_NOFID ((uint32_t)~0)
#define P9_MAXWELEM 16

// Message types
typedef enum {
    Tversion = 100,
    Rversion = 101,
    Tauth = 102,
    Rauth = 103,
    Tattach = 104,
    Rattach = 105,
    Terror = 106,  // Illegal
    Rerror = 107,
    Tflush = 108,
    Rflush = 109,
    Twalk = 110,
    Rwalk = 111,
    Topen = 112,
    Ropen = 113,
    Tcreate = 114,
    Rcreate = 115,
    Tread = 116,
    Rread = 117,
    Twrite = 118,
    Rwrite = 119,
    Tclunk = 120,
    Rclunk = 121,
    Tremove = 122,
    Rremove = 123,
    Tstat = 124,
    Rstat = 125,
    Twstat = 126,
    Rwstat = 127
} p9_msg_type_t;

// QID types
#define P9_QTDIR     0x80  // Directory
#define P9_QTAPPEND  0x40  // Append only
#define P9_QTEXCL    0x20  // Exclusive use
#define P9_QTMOUNT   0x10  // Mounted channel
#define P9_QTAUTH    0x08  // Authentication file
#define P9_QTTMP     0x04  // Temporary file
#define P9_QTFILE    0x00  // Plain file

// Open modes
#define P9_OREAD     0x00  // Read
#define P9_OWRITE    0x01  // Write
#define P9_ORDWR     0x02  // Read/Write
#define P9_OEXEC     0x03  // Execute
#define P9_OTRUNC    0x10  // Truncate
#define P9_OCEXEC    0x20  // Close on exec
#define P9_ORCLOSE   0x40  // Remove on close

// Permission bits (Unix style)
#define P9_DMDIR     0x80000000  // Directory
#define P9_DMAPPEND  0x40000000  // Append only
#define P9_DMEXCL    0x20000000  // Exclusive use
#define P9_DMMOUNT   0x10000000  // Mounted channel
#define P9_DMAUTH    0x08000000  // Authentication file
#define P9_DMTMP     0x04000000  // Temporary file

// QID structure
typedef struct {
    uint8_t type;
    uint32_t version;
    uint64_t path;
} __attribute__((packed)) p9_qid_t;

// Stat structure (9P2000.u)
typedef struct {
    uint16_t size;      // Total size of stat structure
    uint16_t type;      // Server type
    uint32_t dev;       // Server subtype
    p9_qid_t qid;       // Unique file ID
    uint32_t mode;      // Permissions and flags
    uint32_t atime;     // Last access time
    uint32_t mtime;     // Last modification time
    uint64_t length;    // File length in bytes
    char *name;         // File name
    char *uid;          // Owner name
    char *gid;          // Group name
    char *muid;         // Last modifier name
    char *extension;    // Extension string (9P2000.u)
    uint32_t n_uid;     // Numeric UID (9P2000.u)
    uint32_t n_gid;     // Numeric GID (9P2000.u)
    uint32_t n_muid;    // Numeric modifier UID (9P2000.u)
} p9_stat_t;

// String structure for protocol
typedef struct {
    uint16_t len;
    char *str;
} p9_string_t;

// Message buffer
typedef struct {
    uint32_t size;
    uint8_t type;
    uint16_t tag;
    uint8_t *data;
    uint32_t data_len;
    uint32_t data_pos;
} p9_msg_t;

// Encoding/Decoding functions
uint8_t p9_read_u8(p9_msg_t *msg);
uint16_t p9_read_u16(p9_msg_t *msg);
uint32_t p9_read_u32(p9_msg_t *msg);
uint64_t p9_read_u64(p9_msg_t *msg);
void p9_read_string(p9_msg_t *msg, p9_string_t *str);
void p9_read_qid(p9_msg_t *msg, p9_qid_t *qid);
void p9_read_stat(p9_msg_t *msg, p9_stat_t *stat);

void p9_write_u8(p9_msg_t *msg, uint8_t val);
void p9_write_u16(p9_msg_t *msg, uint16_t val);
void p9_write_u32(p9_msg_t *msg, uint32_t val);
void p9_write_u64(p9_msg_t *msg, uint64_t val);
void p9_write_string(p9_msg_t *msg, const char *str);
void p9_write_qid(p9_msg_t *msg, const p9_qid_t *qid);
void p9_write_stat(p9_msg_t *msg, const p9_stat_t *stat);

// Message initialization
void p9_msg_init_request(p9_msg_t *msg, uint8_t *buffer, uint32_t size);
void p9_msg_init_response(p9_msg_t *msg, uint8_t *buffer, uint8_t type, uint16_t tag);
void p9_msg_finalize(p9_msg_t *msg);

#endif // PICOCALC_9P_PROTO_H
```

### 1.2 Protocol Message Handlers

```c
// Message handler function pointer type
typedef int (*p9_handler_t)(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp);

// Handler dispatch table
typedef struct {
    uint8_t type;
    p9_handler_t handler;
    const char *name;
} p9_handler_entry_t;

// Handler functions
int p9_handle_version(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp);
int p9_handle_auth(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp);
int p9_handle_attach(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp);
int p9_handle_flush(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp);
int p9_handle_walk(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp);
int p9_handle_open(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp);
int p9_handle_create(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp);
int p9_handle_read(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp);
int p9_handle_write(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp);
int p9_handle_clunk(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp);
int p9_handle_remove(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp);
int p9_handle_stat(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp);
int p9_handle_wstat(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp);
```

---

## Phase 2: Thread-Safe FAT32 Layer

### 2.1 Synchronization Wrapper (picocalc_fat32_sync.h)

```c
#ifndef PICOCALC_FAT32_SYNC_H
#define PICOCALC_FAT32_SYNC_H

#include "fat32.h"
#include "pico/mutex.h"

// Initialize synchronization
void fat32_sync_init(void);

// Thread-safe wrappers for all FAT32 operations
fat32_error_t fat32_sync_open(fat32_file_t *file, const char *path);
fat32_error_t fat32_sync_create(fat32_file_t *file, const char *path);
fat32_error_t fat32_sync_close(fat32_file_t *file);
fat32_error_t fat32_sync_read(fat32_file_t *file, void *buffer, size_t size, size_t *bytes_read);
fat32_error_t fat32_sync_write(fat32_file_t *file, const void *buffer, size_t size, size_t *bytes_written);
fat32_error_t fat32_sync_seek(fat32_file_t *file, uint32_t position);
fat32_error_t fat32_sync_delete(const char *path);
fat32_error_t fat32_sync_rename(const char *old_path, const char *new_path);
fat32_error_t fat32_sync_dir_read(fat32_file_t *dir, fat32_entry_t *entry);
fat32_error_t fat32_sync_dir_create(fat32_file_t *dir, const char *path);
fat32_error_t fat32_sync_get_free_space(uint64_t *free_space);
fat32_error_t fat32_sync_get_total_space(uint64_t *total_space);

// Lock management (for batch operations)
bool fat32_sync_lock(uint32_t timeout_ms);
void fat32_sync_unlock(void);

#endif // PICOCALC_FAT32_SYNC_H
```

### 2.2 Implementation Pattern

```c
// Example implementation
fat32_error_t fat32_sync_open(fat32_file_t *file, const char *path) {
    if (!fat32_sync_lock(5000)) {
        return FAT32_ERROR_TIMEOUT;
    }
    
    fat32_error_t result = fat32_open(file, path);
    
    fat32_sync_unlock();
    return result;
}
```

---

## Phase 3: Core Server Infrastructure

### 3.1 Server State (picocalc_9p.h)

```c
#ifndef PICOCALC_9P_H
#define PICOCALC_9P_H

#include "picocalc_9p_proto.h"
#include "fat32.h"
#include "lwip/tcp.h"
#include "pico/mutex.h"

// Configuration
#define P9_SERVER_PORT 564
#define P9_MAX_CLIENTS 3
#define P9_MAX_MSG_SIZE 8192
#define P9_MAX_FIDS_PER_CLIENT 32
#define P9_BUFFER_POOL_SIZE 6
#define P9_HOSTNAME "picocalc"

// FID entry
typedef struct {
    uint32_t fid;
    fat32_file_t file;
    p9_qid_t qid;
    bool is_open;
    bool is_dir;
    char path[FAT32_MAX_PATH_LEN];
} p9_fid_entry_t;

// FID table
typedef struct {
    p9_fid_entry_t entries[P9_MAX_FIDS_PER_CLIENT];
    uint32_t count;
} p9_fid_table_t;

// Client state
typedef struct {
    struct tcp_pcb *pcb;
    uint16_t client_id;
    uint8_t *rx_buffer;
    uint32_t rx_len;
    uint32_t rx_expected;
    p9_fid_table_t fids;
    uint32_t max_msg_size;
    char version[32];
    bool active;
} p9_client_t;

// Buffer pool
typedef struct {
    uint8_t *buffers[P9_BUFFER_POOL_SIZE];
    bool in_use[P9_BUFFER_POOL_SIZE];
    mutex_t lock;
} p9_buffer_pool_t;

// Server state
typedef struct {
    bool running;
    struct tcp_pcb *listen_pcb;
    p9_client_t clients[P9_MAX_CLIENTS];
    p9_buffer_pool_t buffer_pool;
    uint32_t next_client_id;
} p9_server_t;

// Server lifecycle
void p9_server_init(void);
void p9_server_start(void);
void p9_server_stop(void);
void p9_server_run(void);
bool p9_server_is_running(void);

// Client management
p9_client_t* p9_client_alloc(struct tcp_pcb *pcb);
void p9_client_free(p9_client_t *client);
void p9_client_close(p9_client_t *client);

// FID management
p9_fid_entry_t* p9_fid_alloc(p9_client_t *client, uint32_t fid);
p9_fid_entry_t* p9_fid_get(p9_client_t *client, uint32_t fid);
void p9_fid_free(p9_client_t *client, uint32_t fid);
void p9_fid_free_all(p9_client_t *client);

// Buffer management
uint8_t* p9_buffer_alloc(void);
void p9_buffer_free(uint8_t *buffer);

// Message processing
void p9_process_message(p9_client_t *client, p9_msg_t *req);
void p9_send_error(p9_client_t *client, uint16_t tag, const char *ename, uint32_t errno_val);

#endif // PICOCALC_9P_H
```

### 3.2 TCP Callbacks

```c
// Accept callback
static err_t p9_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err);

// Receive callback
static err_t p9_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);

// Error callback
static void p9_err_cb(void *arg, err_t err);

// Sent callback
static err_t p9_sent_cb(void *arg, struct tcp_pcb *tpcb, u16_t len);
```

---

## Phase 4: Filesystem Operations

### 4.1 QID Generation

```c
// Generate QID from FAT32 entry
void p9_generate_qid(const fat32_entry_t *entry, p9_qid_t *qid) {
    // Type from attributes
    if (entry->attr & FAT32_ATTR_DIRECTORY) {
        qid->type = P9_QTDIR;
    } else {
        qid->type = P9_QTFILE;
    }
    
    // Path from cluster number and offset
    qid->path = ((uint64_t)entry->start_cluster << 32) | entry->offset;
    
    // Version from modification time
    qid->version = ((uint32_t)entry->date << 16) | entry->time;
}
```

### 4.2 Stat Conversion

```c
// Convert FAT32 entry to 9P stat
void p9_fat32_to_stat(const fat32_entry_t *entry, p9_stat_t *stat) {
    // Basic fields
    stat->type = 0;
    stat->dev = 0;
    p9_generate_qid(entry, &stat->qid);
    
    // Mode from attributes
    stat->mode = 0644;  // Default: rw-r--r--
    if (entry->attr & FAT32_ATTR_DIRECTORY) {
        stat->mode |= P9_DMDIR | 0111;  // Add directory and execute bits
    }
    if (entry->attr & FAT32_ATTR_READ_ONLY) {
        stat->mode &= ~0222;  // Remove write bits
    }
    
    // Times (convert FAT32 format to Unix timestamp)
    stat->atime = p9_fat32_time_to_unix(entry->date, entry->time);
    stat->mtime = stat->atime;
    
    // Size
    stat->length = entry->size;
    
    // Names
    stat->name = entry->filename;
    stat->uid = "picocalc";
    stat->gid = "picocalc";
    stat->muid = "picocalc";
    
    // Unix extensions
    stat->extension = "";
    stat->n_uid = 1000;
    stat->n_gid = 1000;
    stat->n_muid = 1000;
}
```

### 4.3 Walk Implementation

```c
int p9_handle_walk(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp) {
    uint32_t fid = p9_read_u32(req);
    uint32_t newfid = p9_read_u32(req);
    uint16_t nwname = p9_read_u16(req);
    
    // Get source FID
    p9_fid_entry_t *src_fid = p9_fid_get(client, fid);
    if (!src_fid) {
        p9_send_error(client, req->tag, "unknown fid", EBADF);
        return -1;
    }
    
    // Allocate new FID
    p9_fid_entry_t *new_fid = p9_fid_alloc(client, newfid);
    if (!new_fid) {
        p9_send_error(client, req->tag, "fid in use", EEXIST);
        return -1;
    }
    
    // Copy source path
    strncpy(new_fid->path, src_fid->path, sizeof(new_fid->path));
    
    // Walk path components
    p9_qid_t qids[P9_MAXWELEM];
    uint16_t nwqid = 0;
    
    for (uint16_t i = 0; i < nwname && i < P9_MAXWELEM; i++) {
        p9_string_t wname;
        p9_read_string(req, &wname);
        
        // Append to path
        if (strcmp(wname.str, "..") == 0) {
            // Go up one level
            char *last_slash = strrchr(new_fid->path, '/');
            if (last_slash && last_slash != new_fid->path) {
                *last_slash = '\0';
            }
        } else if (strcmp(wname.str, ".") != 0) {
            // Append component
            strncat(new_fid->path, "/", sizeof(new_fid->path) - strlen(new_fid->path) - 1);
            strncat(new_fid->path, wname.str, sizeof(new_fid->path) - strlen(new_fid->path) - 1);
        }
        
        // Check if path exists
        fat32_entry_t entry;
        fat32_error_t err = fat32_sync_open(&new_fid->file, new_fid->path);
        if (err != FAT32_OK) {
            // Partial walk - return what we have
            break;
        }
        
        // Generate QID
        p9_generate_qid(&entry, &qids[nwqid]);
        nwqid++;
        
        fat32_sync_close(&new_fid->file);
    }
    
    // Build response
    p9_write_u16(resp, nwqid);
    for (uint16_t i = 0; i < nwqid; i++) {
        p9_write_qid(resp, &qids[i]);
    }
    
    return 0;
}
```

---

## Phase 5: mDNS Service Discovery

### 5.1 mDNS Responder (picocalc_mdns.h)

```c
#ifndef PICOCALC_MDNS_H
#define PICOCALC_MDNS_H

#include <stdint.h>
#include <stdbool.h>

// mDNS configuration
#define MDNS_PORT 5353
#define MDNS_MULTICAST_ADDR "224.0.0.251"
#define MDNS_TTL 120

// Service types
#define MDNS_SERVICE_9P "_9p._tcp.local"

// Initialize mDNS responder
void mdns_init(const char *hostname);

// Start/stop mDNS responder
void mdns_start(void);
void mdns_stop(void);

// Register service
void mdns_register_service(const char *service, uint16_t port, const char *txt);

// Process mDNS queries (call from network loop)
void mdns_process(void);

#endif // PICOCALC_MDNS_H
```

### 5.2 Basic Implementation

```c
// Simplified mDNS responder
void mdns_init(const char *hostname) {
    // Create UDP PCB for mDNS
    struct udp_pcb *pcb = udp_new();
    udp_bind(pcb, IP_ADDR_ANY, MDNS_PORT);
    
    // Join multicast group
    ip4_addr_t multicast_addr;
    ip4addr_aton(MDNS_MULTICAST_ADDR, &multicast_addr);
    igmp_joingroup(IP_ADDR_ANY, &multicast_addr);
    
    // Set receive callback
    udp_recv(pcb, mdns_recv_callback, NULL);
}

void mdns_register_service(const char *service, uint16_t port, const char *txt) {
    // Store service information for responses
    // Format: hostname._9p._tcp.local
}

void mdns_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                        const ip_addr_t *addr, u16_t port) {
    // Parse mDNS query
    // If query matches our hostname or service, send response
    // Response includes A record and SRV record
}
```

---

## Phase 6: Integration

### 6.1 Core 1 Entry Point

```c
// In main.c
void core1_entry(void) {
    // Wait for WiFi connection
    while (!wifi_is_connected()) {
        sleep_ms(100);
    }
    
    DEBUG_PRINTF("[9P] Starting server on Core 1\n");
    
    // Initialize FAT32 synchronization
    fat32_sync_init();
    
    // Initialize 9P server
    p9_server_init();
    
    // Initialize mDNS
    mdns_init(P9_HOSTNAME);
    mdns_register_service(MDNS_SERVICE_9P, P9_SERVER_PORT, "version=9P2000.u");
    
    // Start server
    p9_server_start();
    
    // Run server loop
    p9_server_run();
}
```

### 6.2 WiFi Connection Callback

```c
// In picocalc_wifi.c
static void wifi_connection_callback(void) {
    if (wifi_is_connected() && !p9_server_is_running()) {
        // Launch 9P server on Core 1
        multicore_launch_core1(core1_entry);
    }
}
```

### 6.3 CMakeLists.txt Updates

```cmake
# Add 9P server sources
add_executable(load81_picocalc
    # ... existing sources ...
    src/picocalc_9p.c
    src/picocalc_9p_proto.c
    src/picocalc_9p_fs.c
    src/picocalc_fat32_sync.c
    src/picocalc_mdns.c
)

# Add multicore support
target_link_libraries(load81_picocalc
    # ... existing libraries ...
    pico_multicore
)
```

---

## Testing Checklist

### Unit Tests
- [ ] Protocol message encoding/decoding
- [ ] QID generation
- [ ] FID table management
- [ ] Buffer pool allocation
- [ ] FAT32 synchronization

### Integration Tests
- [ ] Single client connection
- [ ] Version negotiation
- [ ] Attach to root
- [ ] Walk directory tree
- [ ] Open/read/close file
- [ ] Create/write/close file
- [ ] Directory listing
- [ ] Multiple concurrent clients

### System Tests
- [ ] Mount from Linux: `mount -t 9p -o trans=tcp,port=564 192.168.1.x /mnt/picocalc`
- [ ] List files: `ls -la /mnt/picocalc`
- [ ] Read file: `cat /mnt/picocalc/test.txt`
- [ ] Write file: `echo "test" > /mnt/picocalc/new.txt`
- [ ] Create directory: `mkdir /mnt/picocalc/newdir`
- [ ] Delete file: `rm /mnt/picocalc/test.txt`
- [ ] Concurrent access from multiple terminals
- [ ] Large file transfer (> 1MB)
- [ ] SD card removal during operation
- [ ] WiFi disconnect/reconnect

---

## Debugging Tips

### Enable Debug Output
```c
#define P9_DEBUG 1
#define P9_DEBUG_PROTO 1
#define P9_DEBUG_FS 1
```

### Common Issues

1. **Connection refused**: Check firewall, port 564 open
2. **Mount hangs**: Check version negotiation, message size
3. **Permission denied**: Check QID generation, stat conversion
4. **File not found**: Check path handling, walk implementation
5. **Corruption**: Check mutex protection, buffer management

### Linux Client Debug
```bash
# Enable 9P debug in kernel
echo 1 > /sys/module/9p/parameters/debug
echo 1 > /sys/module/9pnet/parameters/debug

# Mount with debug
mount -t 9p -o trans=tcp,port=564,debug=0xffff 192.168.1.x /mnt/picocalc

# Check kernel logs
dmesg | grep 9p
```

---

## Performance Optimization

### Bottlenecks
1. SD card SPI speed (~1-2 MB/s)
2. FAT32 cluster chain traversal
3. Network packet processing
4. Mutex contention

### Optimizations
1. Increase SPI clock speed if stable
2. Cache directory entries
3. Use larger TCP buffers
4. Batch FAT32 operations
5. Optimize message parsing

---

## Security Considerations

### Current Model
- No authentication
- No encryption
- Full read/write access
- Suitable for trusted networks only

### Future Enhancements
- Add basic authentication
- TLS support
- Read-only mode
- Per-directory permissions

---

## Next Steps

After completing the implementation:

1. **Test thoroughly** with various Linux distributions
2. **Optimize performance** based on profiling
3. **Add configuration UI** in PicoCalc menu
4. **Document usage** for end users
5. **Consider 9P2000.L** extensions for better Linux integration