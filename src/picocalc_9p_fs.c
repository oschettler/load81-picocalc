/**
 * @file picocalc_9p_fs.c
 * @brief 9P Filesystem Operations - FAT32 to 9P Mapping
 * 
 * Provides filesystem operations that map between 9P protocol
 * and FAT32 filesystem, including path resolution, QID generation,
 * and stat structure conversion.
 */

#include "picocalc_9p.h"
#include "picocalc_9p_proto.h"
#include "picocalc_fat32_sync.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ========================================================================
 * Path Utilities
 * ======================================================================== */

/**
 * @brief Normalize path (remove .., ., multiple slashes)
 */
static bool p9_normalize_path(const char *path, char *normalized, size_t size) {
    if (!path || !normalized || size == 0) {
        return false;
    }
    
    /* Start with root */
    normalized[0] = '/';
    size_t out_pos = 1;
    
    const char *p = path;
    while (*p && out_pos < size - 1) {
        /* Skip leading slashes */
        while (*p == '/') p++;
        
        if (*p == '\0') break;
        
        /* Extract component */
        const char *start = p;
        while (*p && *p != '/') p++;
        size_t len = p - start;
        
        if (len == 0) continue;
        
        /* Handle . and .. */
        if (len == 1 && start[0] == '.') {
            continue;  /* Skip current directory */
        }
        
        if (len == 2 && start[0] == '.' && start[1] == '.') {
            /* Go up one directory */
            if (out_pos > 1) {
                out_pos--;  /* Remove trailing slash */
                while (out_pos > 1 && normalized[out_pos - 1] != '/') {
                    out_pos--;
                }
            }
            continue;
        }
        
        /* Add component */
        if (out_pos > 1) {
            normalized[out_pos++] = '/';
        }
        
        if (out_pos + len >= size) {
            return false;  /* Path too long */
        }
        
        memcpy(normalized + out_pos, start, len);
        out_pos += len;
    }
    
    /* Ensure we have at least root */
    if (out_pos == 0) {
        normalized[0] = '/';
        out_pos = 1;
    }
    
    normalized[out_pos] = '\0';
    return true;
}

/**
 * @brief Join two paths
 */
static bool p9_join_path(const char *base, const char *name, char *result, size_t size) {
    if (!base || !name || !result || size == 0) {
        return false;
    }
    
    size_t base_len = strlen(base);
    size_t name_len = strlen(name);
    
    if (base_len + name_len + 2 > size) {
        return false;  /* Result too long */
    }
    
    strcpy(result, base);
    
    /* Add separator if needed */
    if (base_len > 0 && base[base_len - 1] != '/' && name[0] != '/') {
        strcat(result, "/");
    }
    
    strcat(result, name);
    
    /* Normalize the result */
    char temp[FAT32_MAX_PATH_LEN];
    if (!p9_normalize_path(result, temp, sizeof(temp))) {
        return false;
    }
    
    strcpy(result, temp);
    return true;
}

/* ========================================================================
 * QID Generation
 * ======================================================================== */

/**
 * @brief Generate QID from FAT32 entry
 */
static void p9_generate_qid(const fat32_entry_t *entry, uint64_t path, p9_qid_t *qid) {
    qid->type = (entry->attr & FAT32_ATTR_DIRECTORY) ? P9_QTDIR : P9_QTFILE;
    qid->version = 0;  /* FAT32 doesn't have version info */
    qid->path = path;
}

/**
 * @brief Generate QID for root directory
 */
static void p9_generate_root_qid(p9_qid_t *qid) {
    qid->type = P9_QTDIR;
    qid->version = 0;
    qid->path = 1;  /* Root always has path 1 */
}

/* ========================================================================
 * Stat Conversion
 * ======================================================================== */

/**
 * @brief Convert FAT32 date/time to Unix timestamp
 */
static uint32_t p9_fat_to_unix_time(uint16_t date, uint16_t time) {
    /* FAT date: bits 0-4=day, 5-8=month, 9-15=year from 1980 */
    /* FAT time: bits 0-4=seconds/2, 5-10=minutes, 11-15=hours */
    
    int year = 1980 + ((date >> 9) & 0x7F);
    int month = (date >> 5) & 0x0F;
    int day = date & 0x1F;
    
    int hour = (time >> 11) & 0x1F;
    int minute = (time >> 5) & 0x3F;
    int second = (time & 0x1F) * 2;
    
    /* Simple Unix timestamp calculation (not accounting for leap years properly) */
    /* This is approximate but sufficient for 9P */
    uint32_t timestamp = 0;
    
    /* Years since 1970 */
    for (int y = 1970; y < year; y++) {
        timestamp += 365 * 24 * 3600;
        if ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) {
            timestamp += 24 * 3600;  /* Leap year */
        }
    }
    
    /* Months */
    static const int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (int m = 1; m < month; m++) {
        timestamp += days_in_month[m - 1] * 24 * 3600;
    }
    
    /* Days, hours, minutes, seconds */
    timestamp += (day - 1) * 24 * 3600;
    timestamp += hour * 3600;
    timestamp += minute * 60;
    timestamp += second;
    
    return timestamp;
}

/**
 * @brief Convert FAT32 entry to 9P stat structure
 */
void p9_fat_to_stat(const fat32_entry_t *entry, const p9_qid_t *qid, 
                    const char *name, p9_stat_t *stat) {
    memset(stat, 0, sizeof(p9_stat_t));
    
    stat->type = 0;
    stat->dev = 0;
    stat->qid = *qid;
    
    /* Mode: Unix permissions */
    if (entry->attr & FAT32_ATTR_DIRECTORY) {
        stat->mode = 0040755;  /* drwxr-xr-x */
    } else {
        if (entry->attr & FAT32_ATTR_READ_ONLY) {
            stat->mode = 0100444;  /* -r--r--r-- */
        } else {
            stat->mode = 0100644;  /* -rw-r--r-- */
        }
    }
    
    /* Timestamps */
    stat->atime = p9_fat_to_unix_time(entry->date, entry->time);
    stat->mtime = stat->atime;
    
    /* Size */
    stat->length = entry->size;
    
    /* Name */
    stat->name.len = strlen(name);
    stat->name.str = strdup(name);
    
    /* User/group (use defaults) */
    stat->uid.len = 8;
    stat->uid.str = strdup("picocalc");
    stat->gid.len = 8;
    stat->gid.str = strdup("picocalc");
    stat->muid.len = 8;
    stat->muid.str = strdup("picocalc");
    
    /* Extension (empty for FAT32) */
    stat->extension.len = 0;
    stat->extension.str = NULL;
    
    /* Numeric IDs */
    stat->n_uid = 1000;
    stat->n_gid = 1000;
    stat->n_muid = 1000;
}

/**
 * @brief Convert FAT32 file handle to 9P stat structure
 */
void p9_file_to_stat(const fat32_file_t *file, const p9_qid_t *qid,
                     const char *name, p9_stat_t *stat) {
    memset(stat, 0, sizeof(p9_stat_t));
    
    stat->type = 0;
    stat->dev = 0;
    stat->qid = *qid;
    
    /* Mode */
    if (file->attributes & FAT32_ATTR_DIRECTORY) {
        stat->mode = 0040755;
    } else {
        if (file->attributes & FAT32_ATTR_READ_ONLY) {
            stat->mode = 0100444;
        } else {
            stat->mode = 0100644;
        }
    }
    
    /* Timestamps (use current time as approximation) */
    stat->atime = 0;  /* Would need to read from directory entry */
    stat->mtime = 0;
    
    /* Size */
    stat->length = file->file_size;
    
    /* Name */
    stat->name.len = strlen(name);
    stat->name.str = strdup(name);
    
    /* User/group */
    stat->uid.len = 8;
    stat->uid.str = strdup("picocalc");
    stat->gid.len = 8;
    stat->gid.str = strdup("picocalc");
    stat->muid.len = 8;
    stat->muid.str = strdup("picocalc");
    
    /* Extension (empty for FAT32) */
    stat->extension.len = 0;
    stat->extension.str = NULL;
    
    /* Numeric IDs */
    stat->n_uid = 1000;
    stat->n_gid = 1000;
    stat->n_muid = 1000;
}

/* ========================================================================
 * Directory Reading
 * ======================================================================== */

/**
 * @brief Encode directory entry for Tread response
 * 
 * @param entry FAT32 directory entry
 * @param qid QID for this entry
 * @param msg Message buffer to write to
 * @return true on success, false if buffer full
 */
bool p9_encode_dirent(const fat32_entry_t *entry, const p9_qid_t *qid, p9_msg_t *msg) {
    /* Calculate stat size */
    p9_stat_t stat;
    p9_fat_to_stat(entry, qid, entry->filename, &stat);
    
    uint32_t stat_size = p9_stat_size(&stat);
    
    /* Check if we have space */
    if (msg->pos + stat_size + 2 > msg->size) {
        p9_stat_free(&stat);
        return false;
    }
    
    /* Write stat */
    p9_write_stat(msg, &stat);
    
    /* Free stat strings */
    p9_stat_free(&stat);
    
    return true;
}

/* ========================================================================
 * Path Resolution
 * ======================================================================== */

/**
 * @brief Walk path and resolve to FID
 * 
 * @param base_fid Starting FID
 * @param names Array of path components
 * @param nnames Number of path components
 * @param qids Array to store QIDs (output)
 * @param table FID table for QID generation
 * @return Number of components successfully walked, or -1 on error
 */
int p9_walk_path(p9_fid_t *base_fid, const char **names, uint16_t nnames,
                 p9_qid_t *qids, p9_fid_table_t *table) {
    char current_path[FAT32_MAX_PATH_LEN];
    strncpy(current_path, base_fid->path, sizeof(current_path) - 1);
    current_path[sizeof(current_path) - 1] = '\0';
    
    for (uint16_t i = 0; i < nnames; i++) {
        /* Join path */
        char new_path[FAT32_MAX_PATH_LEN];
        if (!p9_join_path(current_path, names[i], new_path, sizeof(new_path))) {
            return i;  /* Path too long */
        }
        
        /* Check if path exists */
        fat32_file_t file;
        fat32_error_t err = fat32_sync_open(&file, new_path);
        
        if (err != FAT32_OK) {
            return i;  /* Path not found */
        }
        
        /* Generate QID */
        qids[i].type = (file.attributes & FAT32_ATTR_DIRECTORY) ? P9_QTDIR : P9_QTFILE;
        qids[i].version = 0;
        qids[i].path = p9_fid_next_qid_path(table);
        
        fat32_sync_close(&file);
        
        /* Update current path */
        strcpy(current_path, new_path);
    }
    
    return nnames;  /* All components walked successfully */
}

/* ========================================================================
 * File Operations
 * ======================================================================== */

/**
 * @brief Open file or directory
 */
fat32_error_t p9_open_file(p9_fid_t *fid, uint8_t mode) {
    fat32_error_t err;
    
    if (fid->type == P9_FID_TYPE_DIR) {
        /* Open directory */
        err = fat32_sync_open(&fid->file, fid->path);
    } else {
        /* Open file */
        err = fat32_sync_open(&fid->file, fid->path);
    }
    
    if (err == FAT32_OK) {
        fid->mode = mode;
        fid->iounit = 8192;  /* Optimal I/O size */
    }
    
    return err;
}

/**
 * @brief Create file or directory
 */
fat32_error_t p9_create_file(p9_fid_t *fid, const char *name, uint32_t perm, uint8_t mode) {
    char new_path[FAT32_MAX_PATH_LEN];
    
    if (!p9_join_path(fid->path, name, new_path, sizeof(new_path))) {
        return FAT32_ERROR_INVALID_PATH;
    }
    
    fat32_error_t err;
    
    if (perm & 0040000) {
        /* Create directory */
        err = fat32_sync_dir_create(&fid->file, new_path);
        fid->type = P9_FID_TYPE_DIR;
    } else {
        /* Create file */
        err = fat32_sync_create(&fid->file, new_path);
        fid->type = P9_FID_TYPE_FILE;
    }
    
    if (err == FAT32_OK) {
        strncpy(fid->path, new_path, sizeof(fid->path) - 1);
        fid->path[sizeof(fid->path) - 1] = '\0';
        fid->mode = mode;
        fid->iounit = 8192;
    }
    
    return err;
}

/**
 * @brief Read from file or directory
 */
fat32_error_t p9_read_file(p9_fid_t *fid, uint64_t offset, uint32_t count,
                           uint8_t *buffer, uint32_t *bytes_read, p9_fid_table_t *table) {
    if (fid->type == P9_FID_TYPE_DIR) {
        /* Read directory entries */
        *bytes_read = 0;
        p9_msg_t msg;
        p9_msg_init_write(&msg, buffer, count, 0, 0);
        
        while (msg.pos < count) {
            fat32_entry_t entry;
            fat32_error_t err = fat32_sync_dir_read(&fid->file, &entry);
            
            if (err != FAT32_OK) {
                break;  /* End of directory or error */
            }
            
            /* Skip . and .. */
            if (strcmp(entry.filename, ".") == 0 || strcmp(entry.filename, "..") == 0) {
                continue;
            }
            
            /* Generate QID */
            p9_qid_t qid;
            p9_generate_qid(&entry, p9_fid_next_qid_path(table), &qid);
            
            /* Encode entry */
            if (!p9_encode_dirent(&entry, &qid, &msg)) {
                break;  /* Buffer full */
            }
        }
        
        *bytes_read = msg.pos;
        return FAT32_OK;
    } else {
        /* Read file */
        fat32_error_t err = fat32_sync_seek(&fid->file, offset);
        if (err != FAT32_OK) {
            return err;
        }
        
        size_t read;
        err = fat32_sync_read(&fid->file, buffer, count, &read);
        *bytes_read = read;
        return err;
    }
}

/**
 * @brief Write to file
 */
fat32_error_t p9_write_file(p9_fid_t *fid, uint64_t offset, uint32_t count,
                            const uint8_t *buffer, uint32_t *bytes_written) {
    if (fid->type == P9_FID_TYPE_DIR) {
        return FAT32_ERROR_NOT_A_FILE;
    }
    
    fat32_error_t err = fat32_sync_seek(&fid->file, offset);
    if (err != FAT32_OK) {
        return err;
    }
    
    size_t written;
    err = fat32_sync_write(&fid->file, buffer, count, &written);
    *bytes_written = written;
    return err;
}

/**
 * @brief Remove file or directory
 */
fat32_error_t p9_remove_file(const char *path) {
    return fat32_sync_delete(path);
}

/**
 * @brief Get file/directory stat
 */
fat32_error_t p9_stat_file(const char *path, p9_stat_t *stat, p9_fid_table_t *table) {
    fat32_file_t file;
    fat32_error_t err = fat32_sync_open(&file, path);
    
    if (err != FAT32_OK) {
        return err;
    }
    
    /* Generate QID */
    p9_qid_t qid;
    qid.type = (file.attributes & FAT32_ATTR_DIRECTORY) ? P9_QTDIR : P9_QTFILE;
    qid.version = 0;
    qid.path = p9_fid_next_qid_path(table);
    
    /* Extract filename from path */
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    
    /* Convert to stat */
    p9_file_to_stat(&file, &qid, name, stat);
    
    fat32_sync_close(&file);
    return FAT32_OK;
}