/**
 * @file picocalc_9p_proto.c
 * @brief 9P2000.u Protocol Implementation
 * 
 * Implementation of 9P2000.u wire protocol message serialization and
 * deserialization with proper little-endian handling.
 */

#include "picocalc_9p_proto.h"
#include <string.h>
#include <stdlib.h>
#include "debug.h"

/* ========================================================================
 * Message Initialization
 * ======================================================================== */

void p9_msg_init_read(p9_msg_t *msg, uint8_t *buffer, uint32_t size) {
    if (!msg || !buffer || size < 7) {
        return;
    }
    
    msg->data = buffer;
    msg->capacity = size;
    msg->pos = 0;
    msg->error = false;
    
    /* Read header */
    msg->size = p9_read_u32(msg);
    msg->type = p9_read_u8(msg);
    msg->tag = p9_read_u16(msg);
    
    /* Validate size */
    if (msg->size < 7 || msg->size > size) {
        msg->error = true;
    }
}

void p9_msg_init_write(p9_msg_t *msg, uint8_t *buffer, uint32_t capacity,
                       uint8_t type, uint16_t tag) {
    if (!msg || !buffer || capacity < 7) {
        return;
    }
    
    msg->data = buffer;
    msg->capacity = capacity;
    msg->pos = 0;
    msg->error = false;
    msg->type = type;
    msg->tag = tag;
    
    /* Reserve space for header (will be filled in finalize) */
    msg->pos = 7;
    msg->size = 7;
}

void p9_msg_finalize(p9_msg_t *msg) {
    if (!msg || msg->error) {
        return;
    }
    
    /* Update size field */
    msg->size = msg->pos;
    
    /* Write header at beginning */
    uint32_t saved_pos = msg->pos;
    msg->pos = 0;
    
    p9_write_u32(msg, msg->size);
    p9_write_u8(msg, msg->type);
    p9_write_u16(msg, msg->tag);
    
    msg->pos = saved_pos;
}

/* ========================================================================
 * Deserialization (Reading from wire format)
 * ======================================================================== */

uint8_t p9_read_u8(p9_msg_t *msg) {
    if (!msg || msg->error || msg->pos + 1 > msg->capacity) {
        if (msg) msg->error = true;
        return 0;
    }
    
    uint8_t val = msg->data[msg->pos];
    msg->pos += 1;
    return val;
}

uint16_t p9_read_u16(p9_msg_t *msg) {
    if (!msg || msg->error || msg->pos + 2 > msg->capacity) {
        if (msg) msg->error = true;
        return 0;
    }
    
    /* Little-endian */
    uint16_t val = (uint16_t)msg->data[msg->pos] |
                   ((uint16_t)msg->data[msg->pos + 1] << 8);
    msg->pos += 2;
    return val;
}

uint32_t p9_read_u32(p9_msg_t *msg) {
    if (!msg || msg->error || msg->pos + 4 > msg->capacity) {
        if (msg) msg->error = true;
        return 0;
    }
    
    /* Little-endian */
    uint32_t val = (uint32_t)msg->data[msg->pos] |
                   ((uint32_t)msg->data[msg->pos + 1] << 8) |
                   ((uint32_t)msg->data[msg->pos + 2] << 16) |
                   ((uint32_t)msg->data[msg->pos + 3] << 24);
    msg->pos += 4;
    return val;
}

uint64_t p9_read_u64(p9_msg_t *msg) {
    if (!msg || msg->error || msg->pos + 8 > msg->capacity) {
        if (msg) msg->error = true;
        return 0;
    }
    
    /* Little-endian */
    uint64_t val = (uint64_t)msg->data[msg->pos] |
                   ((uint64_t)msg->data[msg->pos + 1] << 8) |
                   ((uint64_t)msg->data[msg->pos + 2] << 16) |
                   ((uint64_t)msg->data[msg->pos + 3] << 24) |
                   ((uint64_t)msg->data[msg->pos + 4] << 32) |
                   ((uint64_t)msg->data[msg->pos + 5] << 40) |
                   ((uint64_t)msg->data[msg->pos + 6] << 48) |
                   ((uint64_t)msg->data[msg->pos + 7] << 56);
    msg->pos += 8;
    return val;
}

bool p9_read_string(p9_msg_t *msg, p9_string_t *str) {
    if (!msg || !str || msg->error) {
        return false;
    }
    
    /* Read length */
    str->len = p9_read_u16(msg);
    if (msg->error) {
        return false;
    }
    
    /* Check bounds */
    if (msg->pos + str->len > msg->capacity) {
        msg->error = true;
        return false;
    }
    
    /* Allocate and copy string */
    if (str->len > 0) {
        str->str = (char *)malloc(str->len + 1);
        if (!str->str) {
            msg->error = true;
            return false;
        }
        memcpy(str->str, &msg->data[msg->pos], str->len);
        str->str[str->len] = '\0';  /* Null-terminate for convenience */
        msg->pos += str->len;
    } else {
        str->str = NULL;
    }
    
    return true;
}

uint16_t p9_read_string_buf(p9_msg_t *msg, char *buffer, size_t bufsize) {
    if (!msg || !buffer || bufsize == 0 || msg->error) {
        return 0;
    }
    
    /* Read length */
    uint16_t len = p9_read_u16(msg);
    if (msg->error) {
        return 0;
    }
    
    /* Check bounds */
    if (msg->pos + len > msg->capacity) {
        msg->error = true;
        return 0;
    }
    
    /* Copy to buffer (truncate if necessary) */
    uint16_t copy_len = (len < bufsize - 1) ? len : (bufsize - 1);
    if (copy_len > 0) {
        memcpy(buffer, &msg->data[msg->pos], copy_len);
    }
    buffer[copy_len] = '\0';
    
    msg->pos += len;  /* Advance by full length even if truncated */
    return len;
}

bool p9_read_qid(p9_msg_t *msg, p9_qid_t *qid) {
    if (!msg || !qid || msg->error) {
        return false;
    }
    
    qid->type = p9_read_u8(msg);
    qid->version = p9_read_u32(msg);
    qid->path = p9_read_u64(msg);
    
    return !msg->error;
}

bool p9_read_stat(p9_msg_t *msg, p9_stat_t *stat) {
    if (!msg || !stat || msg->error) {
        return false;
    }
    
    memset(stat, 0, sizeof(p9_stat_t));
    
    /* Read stat size (includes size field itself) */
    stat->size = p9_read_u16(msg);
    if (msg->error) {
        return false;
    }
    
    /* Read fixed fields */
    stat->type = p9_read_u16(msg);
    stat->dev = p9_read_u32(msg);
    
    if (!p9_read_qid(msg, &stat->qid)) {
        return false;
    }
    
    stat->mode = p9_read_u32(msg);
    stat->atime = p9_read_u32(msg);
    stat->mtime = p9_read_u32(msg);
    stat->length = p9_read_u64(msg);
    
    /* Read string fields */
    if (!p9_read_string(msg, &stat->name) ||
        !p9_read_string(msg, &stat->uid) ||
        !p9_read_string(msg, &stat->gid) ||
        !p9_read_string(msg, &stat->muid)) {
        p9_stat_free(stat);
        return false;
    }
    
    /* 9P2000.u extensions */
    if (!p9_read_string(msg, &stat->extension)) {
        p9_stat_free(stat);
        return false;
    }
    
    stat->n_uid = p9_read_u32(msg);
    stat->n_gid = p9_read_u32(msg);
    stat->n_muid = p9_read_u32(msg);
    
    return !msg->error;
}

bool p9_read_bytes(p9_msg_t *msg, uint8_t *data, uint32_t len) {
    if (!msg || !data || msg->error || msg->pos + len > msg->capacity) {
        if (msg) msg->error = true;
        return false;
    }
    
    memcpy(data, &msg->data[msg->pos], len);
    msg->pos += len;
    return true;
}

/* ========================================================================
 * Serialization (Writing to wire format)
 * ======================================================================== */

bool p9_write_u8(p9_msg_t *msg, uint8_t val) {
    if (!msg || msg->error || msg->pos + 1 > msg->capacity) {
        if (msg) msg->error = true;
        return false;
    }
    
    msg->data[msg->pos] = val;
    msg->pos += 1;
    return true;
}

bool p9_write_u16(p9_msg_t *msg, uint16_t val) {
    if (!msg || msg->error || msg->pos + 2 > msg->capacity) {
        if (msg) msg->error = true;
        return false;
    }
    
    /* Little-endian */
    msg->data[msg->pos] = val & 0xFF;
    msg->data[msg->pos + 1] = (val >> 8) & 0xFF;
    msg->pos += 2;
    return true;
}

bool p9_write_u32(p9_msg_t *msg, uint32_t val) {
    if (!msg || msg->error || msg->pos + 4 > msg->capacity) {
        if (msg) msg->error = true;
        return false;
    }
    
    /* Little-endian */
    msg->data[msg->pos] = val & 0xFF;
    msg->data[msg->pos + 1] = (val >> 8) & 0xFF;
    msg->data[msg->pos + 2] = (val >> 16) & 0xFF;
    msg->data[msg->pos + 3] = (val >> 24) & 0xFF;
    msg->pos += 4;
    return true;
}

bool p9_write_u64(p9_msg_t *msg, uint64_t val) {
    if (!msg || msg->error || msg->pos + 8 > msg->capacity) {
        if (msg) msg->error = true;
        return false;
    }
    
    /* Little-endian */
    msg->data[msg->pos] = val & 0xFF;
    msg->data[msg->pos + 1] = (val >> 8) & 0xFF;
    msg->data[msg->pos + 2] = (val >> 16) & 0xFF;
    msg->data[msg->pos + 3] = (val >> 24) & 0xFF;
    msg->data[msg->pos + 4] = (val >> 32) & 0xFF;
    msg->data[msg->pos + 5] = (val >> 40) & 0xFF;
    msg->data[msg->pos + 6] = (val >> 48) & 0xFF;
    msg->data[msg->pos + 7] = (val >> 56) & 0xFF;
    msg->pos += 8;
    return true;
}

bool p9_write_string(p9_msg_t *msg, const char *str) {
    if (!msg || msg->error) {
        if (msg) msg->error = true;
        return false;
    }
    
    uint16_t len = str ? strlen(str) : 0;
    return p9_write_string_len(msg, str, len);
}

bool p9_write_string_len(p9_msg_t *msg, const char *str, uint16_t len) {
    if (!msg || msg->error || msg->pos + 2 + len > msg->capacity) {
        if (msg) msg->error = true;
        return false;
    }
    
    /* Write length */
    if (!p9_write_u16(msg, len)) {
        return false;
    }
    
    /* Write string data */
    if (len > 0 && str) {
        memcpy(&msg->data[msg->pos], str, len);
        msg->pos += len;
    }
    
    return true;
}

bool p9_write_qid(p9_msg_t *msg, const p9_qid_t *qid) {
    if (!msg || !qid || msg->error) {
        return false;
    }
    
    return p9_write_u8(msg, qid->type) &&
           p9_write_u32(msg, qid->version) &&
           p9_write_u64(msg, qid->path);
}

bool p9_write_stat(p9_msg_t *msg, const p9_stat_t *stat) {
    if (!msg || !stat || msg->error) {
        return false;
    }
    
    /* Calculate stat size */
    uint16_t size = p9_stat_size(stat);
    
    /* Write size */
    if (!p9_write_u16(msg, size)) {
        return false;
    }
    
    /* Write fixed fields */
    if (!p9_write_u16(msg, stat->type) ||
        !p9_write_u32(msg, stat->dev) ||
        !p9_write_qid(msg, &stat->qid) ||
        !p9_write_u32(msg, stat->mode) ||
        !p9_write_u32(msg, stat->atime) ||
        !p9_write_u32(msg, stat->mtime) ||
        !p9_write_u64(msg, stat->length)) {
        return false;
    }
    
    /* Write string fields */
    const char *name = stat->name.str ? stat->name.str : "";
    const char *uid = stat->uid.str ? stat->uid.str : "nobody";
    const char *gid = stat->gid.str ? stat->gid.str : "nobody";
    const char *muid = stat->muid.str ? stat->muid.str : "nobody";
    const char *ext = stat->extension.str ? stat->extension.str : "";
    
    if (!p9_write_string(msg, name) ||
        !p9_write_string(msg, uid) ||
        !p9_write_string(msg, gid) ||
        !p9_write_string(msg, muid) ||
        !p9_write_string(msg, ext)) {
        return false;
    }
    
    /* Write 9P2000.u numeric IDs */
    if (!p9_write_u32(msg, stat->n_uid) ||
        !p9_write_u32(msg, stat->n_gid) ||
        !p9_write_u32(msg, stat->n_muid)) {
        return false;
    }
    
    return true;
}

bool p9_write_bytes(p9_msg_t *msg, const uint8_t *data, uint32_t len) {
    if (!msg || !data || msg->error || msg->pos + len > msg->capacity) {
        if (msg) msg->error = true;
        return false;
    }
    
    memcpy(&msg->data[msg->pos], data, len);
    msg->pos += len;
    return true;
}

/* ========================================================================
 * Utility Functions
 * ======================================================================== */

uint32_t p9_msg_remaining(const p9_msg_t *msg) {
    if (!msg || msg->pos > msg->capacity) {
        return 0;
    }
    return msg->capacity - msg->pos;
}

bool p9_msg_has_error(const p9_msg_t *msg) {
    return msg ? msg->error : true;
}

const char* p9_msg_type_name(uint8_t type) {
    switch (type) {
        case Tversion: return "Tversion";
        case Rversion: return "Rversion";
        case Tauth: return "Tauth";
        case Rauth: return "Rauth";
        case Tattach: return "Tattach";
        case Rattach: return "Rattach";
        case Rerror: return "Rerror";
        case Tflush: return "Tflush";
        case Rflush: return "Rflush";
        case Twalk: return "Twalk";
        case Rwalk: return "Rwalk";
        case Topen: return "Topen";
        case Ropen: return "Ropen";
        case Tcreate: return "Tcreate";
        case Rcreate: return "Rcreate";
        case Tread: return "Tread";
        case Rread: return "Rread";
        case Twrite: return "Twrite";
        case Rwrite: return "Rwrite";
        case Tclunk: return "Tclunk";
        case Rclunk: return "Rclunk";
        case Tremove: return "Tremove";
        case Rremove: return "Rremove";
        case Tstat: return "Tstat";
        case Rstat: return "Rstat";
        case Twstat: return "Twstat";
        case Rwstat: return "Rwstat";
        default: return "Unknown";
    }
}

uint16_t p9_stat_size(const p9_stat_t *stat) {
    if (!stat) {
        return 0;
    }
    
    /* Fixed fields: size(2) + type(2) + dev(4) + qid(13) + mode(4) +
     * atime(4) + mtime(4) + length(8) + n_uid(4) + n_gid(4) + n_muid(4) = 53 */
    uint16_t size = 53;
    
    /* String fields: len(2) + data for each */
    size += 2 + (stat->name.str ? stat->name.len : 0);
    size += 2 + (stat->uid.str ? stat->uid.len : 0);
    size += 2 + (stat->gid.str ? stat->gid.len : 0);
    size += 2 + (stat->muid.str ? stat->muid.len : 0);
    size += 2 + (stat->extension.str ? stat->extension.len : 0);
    
    return size;
}

void p9_stat_free(p9_stat_t *stat) {
    if (!stat) {
        return;
    }
    
    p9_string_free(&stat->name);
    p9_string_free(&stat->uid);
    p9_string_free(&stat->gid);
    p9_string_free(&stat->muid);
    p9_string_free(&stat->extension);
}

void p9_string_free(p9_string_t *str) {
    if (str && str->str) {
        free(str->str);
        str->str = NULL;
        str->len = 0;
    }
}