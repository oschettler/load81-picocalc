/**
 * @file picocalc_9p_handlers.c
 * @brief 9P2000.u Message Handlers
 * 
 * Implements all 13 9P message handlers that process client requests
 * and generate appropriate responses.
 */

#include "picocalc_9p.h"
#include "picocalc_9p_proto.h"
#include "picocalc_fat32_sync.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Forward declarations for filesystem operations (in picocalc_9p_fs.c) */
extern int p9_walk_path(p9_fid_t *base_fid, const char **names, uint16_t nnames,
                        p9_qid_t *qids, p9_fid_table_t *table);
extern fat32_error_t p9_open_file(p9_fid_t *fid, uint8_t mode);
extern fat32_error_t p9_create_file(p9_fid_t *fid, const char *name, uint32_t perm, uint8_t mode);
extern fat32_error_t p9_read_file(p9_fid_t *fid, uint64_t offset, uint32_t count,
                                  uint8_t *buffer, uint32_t *bytes_read, p9_fid_table_t *table);
extern fat32_error_t p9_write_file(p9_fid_t *fid, uint64_t offset, uint32_t count,
                                   const uint8_t *buffer, uint32_t *bytes_written);
extern fat32_error_t p9_remove_file(const char *path);
extern fat32_error_t p9_stat_file(const char *path, p9_stat_t *stat, p9_fid_table_t *table);
extern void p9_fat_to_stat(const fat32_entry_t *entry, const p9_qid_t *qid,
                           const char *name, p9_stat_t *stat);
extern void p9_file_to_stat(const fat32_file_t *file, const p9_qid_t *qid,
                            const char *name, p9_stat_t *stat);

/* ========================================================================
 * Helper Functions
 * ======================================================================== */

static void send_error(p9_msg_t *resp, const char *ename) {
    /* Response header already written, just add error string */
    p9_write_string(resp, ename);
}

static const char *fat32_error_to_string(fat32_error_t err) {
    switch (err) {
        case FAT32_OK: return "success";
        case FAT32_ERROR_NO_CARD: return "no SD card";
        case FAT32_ERROR_INIT_FAILED: return "initialization failed";
        case FAT32_ERROR_READ_FAILED: return "read failed";
        case FAT32_ERROR_WRITE_FAILED: return "write failed";
        case FAT32_ERROR_INVALID_FORMAT: return "invalid format";
        case FAT32_ERROR_NOT_MOUNTED: return "not mounted";
        case FAT32_ERROR_FILE_NOT_FOUND: return "file not found";
        case FAT32_ERROR_INVALID_PATH: return "invalid path";
        case FAT32_ERROR_NOT_A_DIRECTORY: return "not a directory";
        case FAT32_ERROR_NOT_A_FILE: return "not a file";
        case FAT32_ERROR_DIR_NOT_EMPTY: return "directory not empty";
        case FAT32_ERROR_DIR_NOT_FOUND: return "directory not found";
        case FAT32_ERROR_DISK_FULL: return "disk full";
        case FAT32_ERROR_FILE_EXISTS: return "file exists";
        case FAT32_ERROR_INVALID_POSITION: return "invalid position";
        case FAT32_ERROR_INVALID_PARAMETER: return "invalid parameter";
        default: return "unknown error";
    }
}

/* ========================================================================
 * Tversion / Rversion - Protocol Version Negotiation
 * ======================================================================== */

void p9_handle_version(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp) {
    /* Read request */
    uint32_t msize = p9_read_u32(req);
    p9_string_t version_str;
    if (!p9_read_string(req, &version_str)) {
        send_error(resp, "invalid version string");
        return;
    }
    
    char *version = version_str.str;
    
    /* Negotiate message size (use smaller of client and server) */
    if (msize > P9_MAX_MSG_SIZE) {
        msize = P9_MAX_MSG_SIZE;
    }
    client->max_msg_size = msize;
    
    /* Check version */
    if (strcmp(version, "9P2000.u") == 0) {
        strncpy(client->version, version, sizeof(client->version) - 1);
        client->state = P9_CLIENT_STATE_VERSION_NEGOTIATED;
    } else {
        /* Unknown version, return "unknown" */
        strncpy(client->version, "unknown", sizeof(client->version) - 1);
    }
    
    /* Write response */
    p9_write_u32(resp, msize);
    p9_write_string(resp, client->version);
    
    p9_string_free(&version_str);
}

/* ========================================================================
 * Tauth / Rauth - Authentication (Stub)
 * ======================================================================== */

void p9_handle_auth(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp) {
    /* Read request (but ignore) */
    uint32_t afid = p9_read_u32(req);
    p9_string_t uname, aname;
    p9_read_string(req, &uname);
    p9_read_string(req, &aname);
    
    /* We don't support authentication, return error */
    send_error(resp, "authentication not required");
    
    p9_string_free(&uname);
    p9_string_free(&aname);
}

/* ========================================================================
 * Tattach / Rattach - Attach to Root
 * ======================================================================== */

void p9_handle_attach(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp) {
    /* Read request */
    uint32_t fid = p9_read_u32(req);
    uint32_t afid = p9_read_u32(req);
    p9_string_t uname, aname;
    
    if (!p9_read_string(req, &uname) || !p9_read_string(req, &aname)) {
        send_error(resp, "invalid attach parameters");
        return;
    }
    
    /* Allocate FID for root */
    p9_fid_t *root_fid = p9_fid_alloc(&client->fid_table, fid);
    if (!root_fid) {
        send_error(resp, "fid already in use");
        p9_string_free(&uname);
        p9_string_free(&aname);
        return;
    }
    
    /* Initialize root FID */
    root_fid->type = P9_FID_TYPE_DIR;
    strcpy(root_fid->path, "/");
    root_fid->qid.type = P9_QTDIR;
    root_fid->qid.version = 0;
    root_fid->qid.path = 1;  /* Root always has path 1 */
    
    /* Write response */
    p9_write_qid(resp, &root_fid->qid);
    
    client->state = P9_CLIENT_STATE_ATTACHED;
    
    p9_string_free(&uname);
    p9_string_free(&aname);
}

/* ========================================================================
 * Twalk / Rwalk - Walk Directory Tree
 * ======================================================================== */

void p9_handle_walk(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp) {
    /* Read request */
    uint32_t fid = p9_read_u32(req);
    uint32_t newfid = p9_read_u32(req);
    uint16_t nwname = p9_read_u16(req);
    
    /* Get source FID */
    p9_fid_t *src_fid = p9_fid_get(&client->fid_table, fid);
    if (!src_fid) {
        send_error(resp, "unknown fid");
        return;
    }
    
    /* Handle zero-length walk (clone FID) */
    if (nwname == 0) {
        p9_fid_t *new_fid = p9_fid_clone(&client->fid_table, fid, newfid);
        if (!new_fid) {
            send_error(resp, "cannot clone fid");
            return;
        }
        
        /* Write response with zero QIDs */
        p9_write_u16(resp, 0);
        return;
    }
    
    /* Read path components */
    p9_string_t name_strs[P9_MAX_WALK_ELEMENTS];
    const char *names[P9_MAX_WALK_ELEMENTS];
    for (int i = 0; i < nwname && i < P9_MAX_WALK_ELEMENTS; i++) {
        if (!p9_read_string(req, &name_strs[i])) {
            /* Free already allocated names */
            for (int j = 0; j < i; j++) {
                p9_string_free(&name_strs[j]);
            }
            send_error(resp, "invalid path component");
            return;
        }
        names[i] = name_strs[i].str;
    }
    
    /* Walk path */
    p9_qid_t qids[P9_MAX_WALK_ELEMENTS];
    int walked = p9_walk_path(src_fid, names, nwname, qids, &client->fid_table);
    
    /* Free path component strings */
    for (int i = 0; i < nwname; i++) {
        p9_string_free(&name_strs[i]);
    }
    
    if (walked < 0) {
        send_error(resp, "walk failed");
        return;
    }
    
    /* If we walked all components, create new FID */
    if (walked == nwname) {
        p9_fid_t *new_fid = p9_fid_clone(&client->fid_table, fid, newfid);
        if (!new_fid) {
            send_error(resp, "cannot create new fid");
            return;
        }
        
        /* Update new FID with final path and QID */
        char final_path[FAT32_MAX_PATH_LEN];
        strncpy(final_path, src_fid->path, sizeof(final_path) - 1);
        
        for (int i = 0; i < walked; i++) {
            size_t len = strlen(final_path);
            if (len > 0 && final_path[len - 1] != '/') {
                strncat(final_path, "/", sizeof(final_path) - len - 1);
            }
            strncat(final_path, names[i], sizeof(final_path) - strlen(final_path) - 1);
        }
        
        strncpy(new_fid->path, final_path, sizeof(new_fid->path) - 1);
        new_fid->qid = qids[walked - 1];
        new_fid->type = (qids[walked - 1].type & P9_QTDIR) ? P9_FID_TYPE_DIR : P9_FID_TYPE_FILE;
    }
    
    /* Write response */
    p9_write_u16(resp, walked);
    for (int i = 0; i < walked; i++) {
        p9_write_qid(resp, &qids[i]);
    }
}

/* ========================================================================
 * Topen / Ropen - Open File or Directory
 * ======================================================================== */

void p9_handle_open(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp) {
    /* Read request */
    uint32_t fid = p9_read_u32(req);
    uint8_t mode = p9_read_u8(req);
    
    /* Get FID */
    p9_fid_t *file_fid = p9_fid_get(&client->fid_table, fid);
    if (!file_fid) {
        send_error(resp, "unknown fid");
        return;
    }
    
    /* Open file */
    fat32_error_t err = p9_open_file(file_fid, mode);
    if (err != FAT32_OK) {
        send_error(resp, fat32_error_to_string(err));
        return;
    }
    
    /* Write response */
    p9_write_qid(resp, &file_fid->qid);
    p9_write_u32(resp, file_fid->iounit);
}

/* ========================================================================
 * Tcreate / Rcreate - Create File or Directory
 * ======================================================================== */

void p9_handle_create(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp) {
    /* Read request */
    uint32_t fid = p9_read_u32(req);
    p9_string_t name_str;
    uint32_t perm = p9_read_u32(req);
    uint8_t mode = p9_read_u8(req);
    
    if (!p9_read_string(req, &name_str)) {
        send_error(resp, "invalid name");
        return;
    }
    
    const char *name = name_str.str;
    
    /* Get FID (must be a directory) */
    p9_fid_t *dir_fid = p9_fid_get(&client->fid_table, fid);
    if (!dir_fid) {
        p9_string_free(&name_str);
        send_error(resp, "unknown fid");
        return;
    }
    
    if (dir_fid->type != P9_FID_TYPE_DIR) {
        p9_string_free(&name_str);
        send_error(resp, "not a directory");
        return;
    }
    
    /* Create file or directory */
    fat32_error_t err = p9_create_file(dir_fid, name, perm, mode);
    p9_string_free(&name_str);
    
    if (err != FAT32_OK) {
        send_error(resp, fat32_error_to_string(err));
        return;
    }
    
    /* Generate new QID */
    dir_fid->qid.path = p9_fid_next_qid_path(&client->fid_table);
    dir_fid->qid.type = (perm & 0040000) ? P9_QTDIR : P9_QTFILE;
    dir_fid->qid.version = 0;
    
    /* Write response */
    p9_write_qid(resp, &dir_fid->qid);
    p9_write_u32(resp, dir_fid->iounit);
}

/* ========================================================================
 * Tread / Rread - Read File or Directory
 * ======================================================================== */

void p9_handle_read(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp) {
    /* Read request */
    uint32_t fid = p9_read_u32(req);
    uint64_t offset = p9_read_u64(req);
    uint32_t count = p9_read_u32(req);
    
    /* Get FID */
    p9_fid_t *file_fid = p9_fid_get(&client->fid_table, fid);
    if (!file_fid) {
        send_error(resp, "unknown fid");
        return;
    }
    
    if (!file_fid->file.is_open) {
        send_error(resp, "file not open");
        return;
    }
    
    /* Limit count to available buffer space */
    uint32_t max_count = client->max_msg_size - 11;  /* Header + count field */
    if (count > max_count) {
        count = max_count;
    }
    
    /* Allocate temporary buffer for data */
    uint8_t *data_buffer = malloc(count);
    if (!data_buffer) {
        send_error(resp, "out of memory");
        return;
    }
    
    /* Read data */
    uint32_t bytes_read = 0;
    fat32_error_t err = p9_read_file(file_fid, offset, count, data_buffer, &bytes_read, &client->fid_table);
    
    if (err != FAT32_OK && err != FAT32_ERROR_INVALID_POSITION) {
        free(data_buffer);
        send_error(resp, fat32_error_to_string(err));
        return;
    }
    
    /* Write response */
    p9_write_u32(resp, bytes_read);
    if (bytes_read > 0) {
        memcpy(resp->data + resp->pos, data_buffer, bytes_read);
        resp->pos += bytes_read;
    }
    
    free(data_buffer);
}

/* ========================================================================
 * Twrite / Rwrite - Write to File
 * ======================================================================== */

void p9_handle_write(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp) {
    /* Read request */
    uint32_t fid = p9_read_u32(req);
    uint64_t offset = p9_read_u64(req);
    uint32_t count = p9_read_u32(req);
    
    /* Get FID */
    p9_fid_t *file_fid = p9_fid_get(&client->fid_table, fid);
    if (!file_fid) {
        send_error(resp, "unknown fid");
        return;
    }
    
    if (!file_fid->file.is_open) {
        send_error(resp, "file not open");
        return;
    }
    
    if (file_fid->type == P9_FID_TYPE_DIR) {
        send_error(resp, "cannot write to directory");
        return;
    }
    
    /* Get data pointer (data follows count in message) */
    const uint8_t *data = req->data + req->pos;
    
    /* Write data */
    uint32_t bytes_written = 0;
    fat32_error_t err = p9_write_file(file_fid, offset, count, data, &bytes_written);
    
    if (err != FAT32_OK) {
        send_error(resp, fat32_error_to_string(err));
        return;
    }
    
    /* Write response */
    p9_write_u32(resp, bytes_written);
}

/* ========================================================================
 * Tclunk / Rclunk - Close FID
 * ======================================================================== */

void p9_handle_clunk(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp) {
    /* Read request */
    uint32_t fid = p9_read_u32(req);
    
    /* Free FID (this also closes the file) */
    p9_fid_free(&client->fid_table, fid);
    
    /* Response has no additional data beyond header */
}

/* ========================================================================
 * Tremove / Rremove - Remove File or Directory
 * ======================================================================== */

void p9_handle_remove(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp) {
    /* Read request */
    uint32_t fid = p9_read_u32(req);
    
    /* Get FID */
    p9_fid_t *file_fid = p9_fid_get(&client->fid_table, fid);
    if (!file_fid) {
        send_error(resp, "unknown fid");
        return;
    }
    
    /* Remove file */
    fat32_error_t err = p9_remove_file(file_fid->path);
    
    /* Free FID regardless of result */
    p9_fid_free(&client->fid_table, fid);
    
    if (err != FAT32_OK) {
        send_error(resp, fat32_error_to_string(err));
        return;
    }
    
    /* Response has no additional data beyond header */
}

/* ========================================================================
 * Tstat / Rstat - Get File Metadata
 * ======================================================================== */

void p9_handle_stat(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp) {
    /* Read request */
    uint32_t fid = p9_read_u32(req);
    
    /* Get FID */
    p9_fid_t *file_fid = p9_fid_get(&client->fid_table, fid);
    if (!file_fid) {
        send_error(resp, "unknown fid");
        return;
    }
    
    /* Get stat */
    p9_stat_t stat;
    fat32_error_t err = p9_stat_file(file_fid->path, &stat, &client->fid_table);
    
    if (err != FAT32_OK) {
        send_error(resp, fat32_error_to_string(err));
        return;
    }
    
    /* Write response */
    uint32_t stat_size = p9_stat_size(&stat);
    p9_write_u16(resp, stat_size + 2);  /* Size includes the size field itself */
    p9_write_stat(resp, &stat);
    
    /* Free stat strings */
    p9_stat_free(&stat);
}

/* ========================================================================
 * Twstat / Rwstat - Set File Metadata
 * ======================================================================== */

void p9_handle_wstat(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp) {
    /* Read request */
    uint32_t fid = p9_read_u32(req);
    uint16_t stat_size = p9_read_u16(req);
    
    p9_stat_t stat;
    if (!p9_read_stat(req, &stat)) {
        send_error(resp, "invalid stat structure");
        return;
    }
    
    /* Get FID */
    p9_fid_t *file_fid = p9_fid_get(&client->fid_table, fid);
    if (!file_fid) {
        p9_stat_free(&stat);
        send_error(resp, "unknown fid");
        return;
    }
    
    /* FAT32 has limited metadata support */
    /* We can only handle name changes (rename) */
    if (stat.name.str && stat.name.len > 0) {
        /* Extract directory from current path */
        char dir_path[FAT32_MAX_PATH_LEN];
        strncpy(dir_path, file_fid->path, sizeof(dir_path) - 1);
        char *last_slash = strrchr(dir_path, '/');
        if (last_slash) {
            *(last_slash + 1) = '\0';
        }
        
        /* Build new path */
        char new_path[FAT32_MAX_PATH_LEN];
        snprintf(new_path, sizeof(new_path), "%s%s", dir_path, stat.name.str);
        
        /* Rename */
        fat32_error_t err = fat32_sync_rename(file_fid->path, new_path);
        if (err != FAT32_OK) {
            p9_stat_free(&stat);
            send_error(resp, fat32_error_to_string(err));
            return;
        }
        
        /* Update FID path */
        strncpy(file_fid->path, new_path, sizeof(file_fid->path) - 1);
    }
    
    p9_stat_free(&stat);
    
    /* Response has no additional data beyond header */
}

/* ========================================================================
 * Tflush / Rflush - Cancel Pending Request
 * ======================================================================== */

void p9_handle_flush(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp) {
    /* Read request */
    uint16_t oldtag = p9_read_u16(req);
    
    /* We don't support asynchronous operations, so flush is a no-op */
    /* Response has no additional data beyond header */
}