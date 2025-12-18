/**
 * @file picocalc_9p_proto.h
 * @brief 9P2000.u Protocol Definitions and Message Handling
 * 
 * This file defines the 9P2000.u wire protocol structures, message types,
 * and serialization/deserialization functions for the PicoCalc 9P server.
 * 
 * Protocol Reference: http://ericvh.github.io/9p-rfc/rfc9p2000.u.html
 */

#ifndef PICOCALC_9P_PROTO_H
#define PICOCALC_9P_PROTO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Protocol version string */
#define P9_VERSION "9P2000.u"
#define P9_VERSION_LEN 8

/* Special values */
#define P9_NOTAG ((uint16_t)~0)
#define P9_NOFID ((uint32_t)~0)
#define P9_MAXWELEM 16
#define P9_MAX_WALK_ELEMENTS 16

/* Maximum message size (negotiable) */
#define P9_DEFAULT_MSIZE 8192
#define P9_MIN_MSIZE 256
#define P9_MAX_MSIZE 65536

/**
 * @brief 9P2000 Message Types
 */
typedef enum {
    Tversion = 100,  /**< Version negotiation request */
    Rversion = 101,  /**< Version negotiation response */
    Tauth = 102,     /**< Authentication request */
    Rauth = 103,     /**< Authentication response */
    Tattach = 104,   /**< Attach to filesystem root */
    Rattach = 105,   /**< Attach response */
    Terror = 106,    /**< Illegal - never sent */
    Rerror = 107,    /**< Error response */
    Tflush = 108,    /**< Abort pending request */
    Rflush = 109,    /**< Flush response */
    Twalk = 110,     /**< Navigate directory tree */
    Rwalk = 111,     /**< Walk response */
    Topen = 112,     /**< Open file/directory */
    Ropen = 113,     /**< Open response */
    Tcreate = 114,   /**< Create new file */
    Rcreate = 115,   /**< Create response */
    Tread = 116,     /**< Read file data */
    Rread = 117,     /**< Read response */
    Twrite = 118,    /**< Write file data */
    Rwrite = 119,    /**< Write response */
    Tclunk = 120,    /**< Close file handle */
    Rclunk = 121,    /**< Clunk response */
    Tremove = 122,   /**< Delete file */
    Rremove = 123,   /**< Remove response */
    Tstat = 124,     /**< Get file metadata */
    Rstat = 125,     /**< Stat response */
    Twstat = 126,    /**< Set file metadata */
    Rwstat = 127     /**< Wstat response */
} p9_msg_type_t;

/**
 * @brief QID Type Bits
 */
#define P9_QTDIR     0x80  /**< Directory */
#define P9_QTAPPEND  0x40  /**< Append only */
#define P9_QTEXCL    0x20  /**< Exclusive use */
#define P9_QTMOUNT   0x10  /**< Mounted channel */
#define P9_QTAUTH    0x08  /**< Authentication file */
#define P9_QTTMP     0x04  /**< Temporary file */
#define P9_QTFILE    0x00  /**< Plain file */

/**
 * @brief Open Mode Flags
 */
#define P9_OREAD     0x00  /**< Read */
#define P9_OWRITE    0x01  /**< Write */
#define P9_ORDWR     0x02  /**< Read/Write */
#define P9_OEXEC     0x03  /**< Execute */
#define P9_OTRUNC    0x10  /**< Truncate */
#define P9_OCEXEC    0x20  /**< Close on exec */
#define P9_ORCLOSE   0x40  /**< Remove on close */

/**
 * @brief Permission/Mode Bits (Unix style)
 */
#define P9_DMDIR     0x80000000  /**< Directory */
#define P9_DMAPPEND  0x40000000  /**< Append only */
#define P9_DMEXCL    0x20000000  /**< Exclusive use */
#define P9_DMMOUNT   0x10000000  /**< Mounted channel */
#define P9_DMAUTH    0x08000000  /**< Authentication file */
#define P9_DMTMP     0x04000000  /**< Temporary file */

/* Unix permission bits */
#define P9_PERM_MASK 0x01FF  /**< Permission bits mask */

/**
 * @brief QID - Unique File Identifier
 * 
 * The QID uniquely identifies a file on the server.
 */
typedef struct {
    uint8_t type;       /**< File type (QTDIR, QTFILE, etc.) */
    uint32_t version;   /**< Version number for cache coherency */
    uint64_t path;      /**< Unique file identifier */
} __attribute__((packed)) p9_qid_t;

/**
 * @brief 9P String
 * 
 * Strings in 9P are length-prefixed (2-byte length + data).
 */
typedef struct {
    uint16_t len;   /**< String length */
    char *str;      /**< String data (not null-terminated in protocol) */
} p9_string_t;

/**
 * @brief 9P2000.u Stat Structure
 * 
 * File metadata structure with Unix extensions.
 */
typedef struct {
    uint16_t size;      /**< Total size of stat structure */
    uint16_t type;      /**< Server type */
    uint32_t dev;       /**< Server subtype */
    p9_qid_t qid;       /**< Unique file ID */
    uint32_t mode;      /**< Permissions and flags */
    uint32_t atime;     /**< Last access time (Unix timestamp) */
    uint32_t mtime;     /**< Last modification time */
    uint64_t length;    /**< File length in bytes */
    p9_string_t name;   /**< File name */
    p9_string_t uid;    /**< Owner name */
    p9_string_t gid;    /**< Group name */
    p9_string_t muid;   /**< Last modifier name */
    /* 9P2000.u extensions */
    p9_string_t extension;  /**< Extension string */
    uint32_t n_uid;     /**< Numeric UID */
    uint32_t n_gid;     /**< Numeric GID */
    uint32_t n_muid;    /**< Numeric modifier UID */
} p9_stat_t;

/**
 * @brief Message Buffer
 * 
 * Represents a 9P protocol message with serialization state.
 */
typedef struct {
    uint32_t size;      /**< Total message size */
    uint8_t type;       /**< Message type */
    uint16_t tag;       /**< Message tag */
    uint8_t *data;      /**< Message data buffer */
    uint32_t capacity;  /**< Buffer capacity */
    uint32_t pos;       /**< Current read/write position */
    bool error;         /**< Error flag */
} p9_msg_t;

/* ========================================================================
 * Message Initialization
 * ======================================================================== */

/**
 * @brief Initialize message buffer for reading
 * @param msg Message structure to initialize
 * @param buffer Data buffer
 * @param size Buffer size
 */
void p9_msg_init_read(p9_msg_t *msg, uint8_t *buffer, uint32_t size);

/**
 * @brief Initialize message buffer for writing
 * @param msg Message structure to initialize
 * @param buffer Data buffer
 * @param capacity Buffer capacity
 * @param type Message type
 * @param tag Message tag
 */
void p9_msg_init_write(p9_msg_t *msg, uint8_t *buffer, uint32_t capacity,
                       uint8_t type, uint16_t tag);

/**
 * @brief Finalize message (update size field)
 * @param msg Message to finalize
 */
void p9_msg_finalize(p9_msg_t *msg);

/* ========================================================================
 * Deserialization (Reading from wire format)
 * ======================================================================== */

/**
 * @brief Read 8-bit unsigned integer
 * @param msg Message buffer
 * @return Value read, or 0 on error
 */
uint8_t p9_read_u8(p9_msg_t *msg);

/**
 * @brief Read 16-bit unsigned integer (little-endian)
 * @param msg Message buffer
 * @return Value read, or 0 on error
 */
uint16_t p9_read_u16(p9_msg_t *msg);

/**
 * @brief Read 32-bit unsigned integer (little-endian)
 * @param msg Message buffer
 * @return Value read, or 0 on error
 */
uint32_t p9_read_u32(p9_msg_t *msg);

/**
 * @brief Read 64-bit unsigned integer (little-endian)
 * @param msg Message buffer
 * @return Value read, or 0 on error
 */
uint64_t p9_read_u64(p9_msg_t *msg);

/**
 * @brief Read string (allocates memory for string data)
 * @param msg Message buffer
 * @param str String structure to populate
 * @return true on success, false on error
 * @note Caller must free str->str when done
 */
bool p9_read_string(p9_msg_t *msg, p9_string_t *str);

/**
 * @brief Read string into fixed buffer (no allocation)
 * @param msg Message buffer
 * @param buffer Destination buffer
 * @param bufsize Buffer size
 * @return Number of bytes read, or 0 on error
 */
uint16_t p9_read_string_buf(p9_msg_t *msg, char *buffer, size_t bufsize);

/**
 * @brief Read QID structure
 * @param msg Message buffer
 * @param qid QID structure to populate
 * @return true on success, false on error
 */
bool p9_read_qid(p9_msg_t *msg, p9_qid_t *qid);

/**
 * @brief Read stat structure
 * @param msg Message buffer
 * @param stat Stat structure to populate
 * @return true on success, false on error
 * @note Caller must free string fields when done
 */
bool p9_read_stat(p9_msg_t *msg, p9_stat_t *stat);

/**
 * @brief Read raw bytes
 * @param msg Message buffer
 * @param data Destination buffer
 * @param len Number of bytes to read
 * @return true on success, false on error
 */
bool p9_read_bytes(p9_msg_t *msg, uint8_t *data, uint32_t len);

/* ========================================================================
 * Serialization (Writing to wire format)
 * ======================================================================== */

/**
 * @brief Write 8-bit unsigned integer
 * @param msg Message buffer
 * @param val Value to write
 * @return true on success, false on error
 */
bool p9_write_u8(p9_msg_t *msg, uint8_t val);

/**
 * @brief Write 16-bit unsigned integer (little-endian)
 * @param msg Message buffer
 * @param val Value to write
 * @return true on success, false on error
 */
bool p9_write_u16(p9_msg_t *msg, uint16_t val);

/**
 * @brief Write 32-bit unsigned integer (little-endian)
 * @param msg Message buffer
 * @param val Value to write
 * @return true on success, false on error
 */
bool p9_write_u32(p9_msg_t *msg, uint32_t val);

/**
 * @brief Write 64-bit unsigned integer (little-endian)
 * @param msg Message buffer
 * @param val Value to write
 * @return true on success, false on error
 */
bool p9_write_u64(p9_msg_t *msg, uint64_t val);

/**
 * @brief Write string
 * @param msg Message buffer
 * @param str String to write (null-terminated)
 * @return true on success, false on error
 */
bool p9_write_string(p9_msg_t *msg, const char *str);

/**
 * @brief Write string with explicit length
 * @param msg Message buffer
 * @param str String data
 * @param len String length
 * @return true on success, false on error
 */
bool p9_write_string_len(p9_msg_t *msg, const char *str, uint16_t len);

/**
 * @brief Write QID structure
 * @param msg Message buffer
 * @param qid QID to write
 * @return true on success, false on error
 */
bool p9_write_qid(p9_msg_t *msg, const p9_qid_t *qid);

/**
 * @brief Write stat structure
 * @param msg Message buffer
 * @param stat Stat to write
 * @return true on success, false on error
 */
bool p9_write_stat(p9_msg_t *msg, const p9_stat_t *stat);

/**
 * @brief Write raw bytes
 * @param msg Message buffer
 * @param data Source buffer
 * @param len Number of bytes to write
 * @return true on success, false on error
 */
bool p9_write_bytes(p9_msg_t *msg, const uint8_t *data, uint32_t len);

/* ========================================================================
 * Utility Functions
 * ======================================================================== */

/**
 * @brief Get remaining bytes in message buffer
 * @param msg Message buffer
 * @return Number of bytes remaining
 */
uint32_t p9_msg_remaining(const p9_msg_t *msg);

/**
 * @brief Check if message has error
 * @param msg Message buffer
 * @return true if error occurred
 */
bool p9_msg_has_error(const p9_msg_t *msg);

/**
 * @brief Get message type name
 * @param type Message type
 * @return String name of message type
 */
const char* p9_msg_type_name(uint8_t type);

/**
 * @brief Calculate stat structure size
 * @param stat Stat structure
 * @return Size in bytes
 */
uint16_t p9_stat_size(const p9_stat_t *stat);

/**
 * @brief Free stat structure strings
 * @param stat Stat structure
 */
void p9_stat_free(p9_stat_t *stat);

/**
 * @brief Free string structure
 * @param str String structure
 */
void p9_string_free(p9_string_t *str);

#endif /* PICOCALC_9P_PROTO_H */