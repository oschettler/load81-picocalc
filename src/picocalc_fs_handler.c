#include "picocalc_fs_handler.h"
#include "picocalc_file_server.h"
#include "fat32.h"
#include "debug.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

/* Error message strings */
static const char *fs_error_messages[] = {
    "Success",
    "File or directory not found",
    "Not a directory",
    "Not a file",
    "File or directory already exists",
    "No space left on device",
    "I/O error",
    "Invalid path",
    "Out of memory",
    "File too large",
    "SD card not mounted"
};

fs_error_t fs_init(void) {
    /* File system is initialized by main application */
    if (!fat32_is_mounted()) {
        return FS_ERR_NOT_MOUNTED;
    }
    return FS_OK;
}

const char *fs_error_string(fs_error_t error) {
    if (error >= 0 && error < sizeof(fs_error_messages) / sizeof(fs_error_messages[0])) {
        return fs_error_messages[error];
    }
    return "Unknown error";
}

/* Translate FAT32 error to FS error */
static fs_error_t translate_fat32_error(fat32_error_t err) {
    switch (err) {
        case FAT32_OK:
            return FS_OK;
        case FAT32_ERROR_FILE_NOT_FOUND:
        case FAT32_ERROR_DIR_NOT_FOUND:
            return FS_ERR_NOT_FOUND;
        case FAT32_ERROR_NOT_A_DIRECTORY:
            return FS_ERR_NOT_DIR;
        case FAT32_ERROR_NOT_A_FILE:
            return FS_ERR_NOT_FILE;
        case FAT32_ERROR_FILE_EXISTS:
            return FS_ERR_EXISTS;
        case FAT32_ERROR_DISK_FULL:
            return FS_ERR_NO_SPACE;
        case FAT32_ERROR_INVALID_PATH:
        case FAT32_ERROR_INVALID_PARAMETER:
            return FS_ERR_INVALID_PATH;
        case FAT32_ERROR_NOT_MOUNTED:
            return FS_ERR_NOT_MOUNTED;
        default:
            return FS_ERR_IO;
    }
}

/* Normalize path: handle ., .., absolute/relative paths */
fs_error_t fs_normalize_path(const char *path, const char *cwd, 
                             char *out, size_t out_len) {
    if (!path || !cwd || !out || out_len == 0) {
        return FS_ERR_INVALID_PATH;
    }
    
    char temp[256];
    const char *src;
    
    /* Start with absolute path or combine with cwd */
    if (path[0] == '/') {
        src = path;
    } else {
        /* Combine cwd with relative path */
        snprintf(temp, sizeof(temp), "%s/%s", cwd, path);
        src = temp;
    }
    
    /* Normalize: remove ., .., // */
    char *components[32];
    int comp_count = 0;
    char work[256];
    strncpy(work, src, sizeof(work) - 1);
    work[sizeof(work) - 1] = '\0';
    
    char *token = strtok(work, "/");
    while (token && comp_count < 32) {
        if (strcmp(token, ".") == 0) {
            /* Skip current directory */
            continue;
        } else if (strcmp(token, "..") == 0) {
            /* Go up one level */
            if (comp_count > 0) {
                comp_count--;
            }
        } else if (token[0] != '\0') {
            /* Add component */
            components[comp_count++] = token;
        }
        token = strtok(NULL, "/");
    }
    
    /* Build normalized path */
    if (comp_count == 0) {
        /* Root directory */
        strncpy(out, "/", out_len);
    } else {
        out[0] = '\0';
        for (int i = 0; i < comp_count; i++) {
            strncat(out, "/", out_len - strlen(out) - 1);
            strncat(out, components[i], out_len - strlen(out) - 1);
        }
    }
    
    return FS_OK;
}

/* Escape JSON string */
static void json_escape_string(const char *str, char *out, size_t out_len) {
    size_t out_pos = 0;
    for (size_t i = 0; str[i] && out_pos < out_len - 2; i++) {
        char c = str[i];
        if (c == '"' || c == '\\') {
            if (out_pos < out_len - 3) {
                out[out_pos++] = '\\';
                out[out_pos++] = c;
            }
        } else if (c == '\n') {
            if (out_pos < out_len - 3) {
                out[out_pos++] = '\\';
                out[out_pos++] = 'n';
            }
        } else if (c == '\r') {
            if (out_pos < out_len - 3) {
                out[out_pos++] = '\\';
                out[out_pos++] = 'r';
            }
        } else if (c == '\t') {
            if (out_pos < out_len - 3) {
                out[out_pos++] = '\\';
                out[out_pos++] = 't';
            }
        } else if (c >= 32 && c < 127) {
            out[out_pos++] = c;
        }
    }
    out[out_pos] = '\0';
}

fs_error_t fs_list_dir(const char *path, char **json_out) {
    if (!path || !json_out) {
        return FS_ERR_INVALID_PATH;
    }
    
    if (!fat32_is_mounted()) {
        return FS_ERR_NOT_MOUNTED;
    }
    
    /* Open directory */
    fat32_file_t dir;
    fat32_error_t result = fat32_open(&dir, path);
    if (result != FAT32_OK) {
        return translate_fat32_error(result);
    }
    
    /* Check if it's a directory */
    if (!(dir.attributes & FAT32_ATTR_DIRECTORY)) {
        fat32_close(&dir);
        return FS_ERR_NOT_DIR;
    }
    
    /* Allocate buffer for JSON (start with 4KB) */
    size_t json_capacity = 4096;
    char *json = malloc(json_capacity);
    if (!json) {
        fat32_close(&dir);
        return FS_ERR_NO_MEMORY;
    }
    
    /* Start JSON array */
    strcpy(json, "[");
    size_t json_len = 1;
    bool first = true;
    
    /* Read directory entries */
    fat32_entry_t entry;
    while (fat32_dir_read(&dir, &entry) == FAT32_OK) {
        if (!entry.filename[0]) break;  /* End of directory */
        
        /* Skip . and .. */
        if (strcmp(entry.filename, ".") == 0 || strcmp(entry.filename, "..") == 0) {
            continue;
        }
        
        /* Format entry as JSON object */
        char entry_json[512];
        char escaped_name[300];
        json_escape_string(entry.filename, escaped_name, sizeof(escaped_name));
        
        snprintf(entry_json, sizeof(entry_json),
                "%s{\"name\":\"%s\",\"size\":%lu,\"is_dir\":%s}",
                first ? "" : ",",
                escaped_name,
                (unsigned long)entry.size,
                (entry.attr & FAT32_ATTR_DIRECTORY) ? "true" : "false");
        
        /* Check if we need to expand buffer */
        size_t entry_len = strlen(entry_json);
        if (json_len + entry_len + 2 > json_capacity) {
            json_capacity *= 2;
            char *new_json = realloc(json, json_capacity);
            if (!new_json) {
                free(json);
                fat32_close(&dir);
                return FS_ERR_NO_MEMORY;
            }
            json = new_json;
        }
        
        /* Append entry */
        strcat(json, entry_json);
        json_len += entry_len;
        first = false;
    }
    
    /* Close JSON array */
    strcat(json, "]");
    
    fat32_close(&dir);
    *json_out = json;
    return FS_OK;
}

fs_error_t fs_read_file(const char *path, uint8_t **data, size_t *size) {
    if (!path || !data || !size) {
        return FS_ERR_INVALID_PATH;
    }
    
    DEBUG_PRINTF("[FS] fs_read_file: Starting read of '%s'\n", path);
    
    if (!fat32_is_mounted()) {
        DEBUG_PRINTF("[FS] fs_read_file: SD card not mounted\n");
        return FS_ERR_NOT_MOUNTED;
    }
    
    /* Open file */
    DEBUG_PRINTF("[FS] fs_read_file: Opening file...\n");
    fat32_file_t file;
    fat32_error_t result = fat32_open(&file, path);
    if (result != FAT32_OK) {
        DEBUG_PRINTF("[FS] fs_read_file: Open failed with FAT32 error %d\n", result);
        return translate_fat32_error(result);
    }
    DEBUG_PRINTF("[FS] fs_read_file: File opened successfully\n");
    
    /* Check if it's a file */
    if (file.attributes & FAT32_ATTR_DIRECTORY) {
        DEBUG_PRINTF("[FS] fs_read_file: Path is a directory, not a file\n");
        fat32_close(&file);
        return FS_ERR_NOT_FILE;
    }
    
    /* Check file size */
    uint32_t file_size = fat32_size(&file);
    DEBUG_PRINTF("[FS] fs_read_file: File size = %lu bytes\n", (unsigned long)file_size);
    
    if (file_size > FILE_SERVER_MAX_FILE_SIZE) {
        DEBUG_PRINTF("[FS] fs_read_file: File too large (max %lu bytes)\n",
                    (unsigned long)FILE_SERVER_MAX_FILE_SIZE);
        fat32_close(&file);
        return FS_ERR_TOO_LARGE;
    }
    
    /* Allocate buffer for entire file */
    DEBUG_PRINTF("[FS] fs_read_file: Allocating %lu bytes...\n", (unsigned long)file_size);
    uint8_t *buffer = malloc(file_size);
    if (!buffer) {
        DEBUG_PRINTF("[FS] fs_read_file: Memory allocation failed\n");
        fat32_close(&file);
        return FS_ERR_NO_MEMORY;
    }
    DEBUG_PRINTF("[FS] fs_read_file: Buffer allocated successfully\n");
    
    /* Read file in chunks to avoid memory issues */
    size_t total_read = 0;
    size_t chunk_size = 4096;  /* 4KB chunks */
    
    DEBUG_PRINTF("[FS] fs_read_file: Starting chunked read (chunk_size=%lu)...\n",
                (unsigned long)chunk_size);
    
    while (total_read < file_size) {
        size_t to_read = file_size - total_read;
        if (to_read > chunk_size) {
            to_read = chunk_size;
        }
        
        DEBUG_PRINTF("[FS] fs_read_file: Reading chunk at offset %lu, size %lu\n",
                    (unsigned long)total_read, (unsigned long)to_read);
        
        size_t bytes_read;
        result = fat32_read(&file, buffer + total_read, to_read, &bytes_read);
        if (result != FAT32_OK) {
            DEBUG_PRINTF("[FS] fs_read_file: FAT32 read failed with error %d at offset %lu\n",
                        result, (unsigned long)total_read);
            free(buffer);
            fat32_close(&file);
            return translate_fat32_error(result);
        }
        
        DEBUG_PRINTF("[FS] fs_read_file: Read %lu bytes\n", (unsigned long)bytes_read);
        total_read += bytes_read;
        
        /* If we read less than requested, we've reached EOF */
        if (bytes_read < to_read) {
            DEBUG_PRINTF("[FS] fs_read_file: EOF reached\n");
            break;
        }
    }
    
    DEBUG_PRINTF("[FS] fs_read_file: Read complete, total_read=%lu, file_size=%lu\n",
                (unsigned long)total_read, (unsigned long)file_size);
    
    fat32_close(&file);
    DEBUG_PRINTF("[FS] fs_read_file: File closed\n");
    
    *data = buffer;
    *size = total_read;
    DEBUG_PRINTF("[FS] fs_read_file: Success, returning %lu bytes\n", (unsigned long)total_read);
    return FS_OK;
}

fs_error_t fs_get_file_size(const char *path, size_t *size) {
    if (!path || !size) {
        return FS_ERR_INVALID_PATH;
    }
    
    if (!fat32_is_mounted()) {
        return FS_ERR_NOT_MOUNTED;
    }
    
    /* Open file */
    fat32_file_t file;
    fat32_error_t result = fat32_open(&file, path);
    if (result != FAT32_OK) {
        return translate_fat32_error(result);
    }
    
    /* Check if it's a file */
    if (file.attributes & FAT32_ATTR_DIRECTORY) {
        fat32_close(&file);
        return FS_ERR_NOT_FILE;
    }
    
    /* Get file size */
    uint32_t file_size = fat32_size(&file);
    fat32_close(&file);
    
    /* Check size limit */
    if (file_size > FILE_SERVER_MAX_FILE_SIZE) {
        return FS_ERR_TOO_LARGE;
    }
    
    *size = file_size;
    return FS_OK;
}

fs_error_t fs_read_file_chunked(const char *path, fs_read_chunk_callback_t callback, void *user_data) {
    if (!path || !callback) {
        return FS_ERR_INVALID_PATH;
    }
    
    if (!fat32_is_mounted()) {
        DEBUG_PRINTF("[FS] Chunked read: SD card not mounted\n");
        return FS_ERR_NOT_MOUNTED;
    }
    
    DEBUG_PRINTF("[FS] Chunked read: Opening %s\n", path);
    
    /* Open file */
    fat32_file_t file;
    fat32_error_t result = fat32_open(&file, path);
    if (result != FAT32_OK) {
        DEBUG_PRINTF("[FS] Chunked read: Open failed with FAT32 error %d\n", result);
        return translate_fat32_error(result);
    }
    
    DEBUG_PRINTF("[FS] Chunked read: File opened successfully\n");
    
    /* Check if it's a file */
    if (file.attributes & FAT32_ATTR_DIRECTORY) {
        fat32_close(&file);
        return FS_ERR_NOT_FILE;
    }
    
    /* Get file size */
    uint32_t file_size = fat32_size(&file);
    DEBUG_PRINTF("[FS] Chunked read: File size = %lu bytes\n", (unsigned long)file_size);
    
    if (file_size > FILE_SERVER_MAX_FILE_SIZE) {
        fat32_close(&file);
        return FS_ERR_TOO_LARGE;
    }
    
    /* Pass file size to callback context (assuming it's a cat_chunk_context_t) */
    /* This is a bit of a hack, but avoids changing the callback signature */
    typedef struct {
        void *client;
        uint32_t total_size;
        uint32_t bytes_sent;
        bool header_sent;
        bool error_occurred;
    } context_with_size_t;
    
    context_with_size_t *ctx = (context_with_size_t *)user_data;
    if (ctx) {
        ctx->total_size = file_size;
    }
    
    /* Allocate a single chunk buffer (1KB to avoid TCP buffer overflow) */
    #define CHUNK_SIZE 1024
    uint8_t *chunk_buffer = malloc(CHUNK_SIZE);
    if (!chunk_buffer) {
        fat32_close(&file);
        return FS_ERR_NO_MEMORY;
    }
    
    /* Read and send file in chunks */
    size_t total_read = 0;
    fs_error_t error = FS_OK;
    bool callback_aborted = false;
    
    while (total_read < file_size) {
        size_t to_read = file_size - total_read;
        if (to_read > CHUNK_SIZE) {
            to_read = CHUNK_SIZE;
        }
        
        DEBUG_PRINTF("[FS] Chunked read: Reading chunk at offset %lu, size %lu\n",
                    (unsigned long)total_read, (unsigned long)to_read);
        
        size_t bytes_read;
        result = fat32_read(&file, chunk_buffer, to_read, &bytes_read);
        if (result != FAT32_OK) {
            DEBUG_PRINTF("[FS] Chunked read: FAT32 read failed with error %d at offset %lu\n",
                        result, (unsigned long)total_read);
            error = translate_fat32_error(result);
            break;
        }
        
        DEBUG_PRINTF("[FS] Chunked read: Read %lu bytes\n", (unsigned long)bytes_read);
        
        /* Call callback with chunk */
        if (!callback(chunk_buffer, bytes_read, user_data)) {
            /* Callback returned false - abort (e.g., TCP error) */
            DEBUG_PRINTF("[FS] Chunked read: Callback aborted\n");
            callback_aborted = true;
            break;
        }
        
        total_read += bytes_read;
        
        /* If we read less than requested, we've reached EOF */
        if (bytes_read < to_read) {
            DEBUG_PRINTF("[FS] Chunked read: EOF reached\n");
            break;
        }
    }
    
    DEBUG_PRINTF("[FS] Chunked read: Complete, total_read=%lu, file_size=%lu\n",
                (unsigned long)total_read, (unsigned long)file_size);
    
    free(chunk_buffer);
    fat32_close(&file);
    
    /* If callback aborted but no FS error, return I/O error to indicate failure */
    if (callback_aborted && error == FS_OK) {
        DEBUG_PRINTF("[FS] Chunked read: Returning FS_ERR_IO due to callback abort\n");
        return FS_ERR_IO;
    }
    
    if (error != FS_OK) {
        DEBUG_PRINTF("[FS] Chunked read: Returning error %d (%s)\n", error, fs_error_string(error));
    } else {
        DEBUG_PRINTF("[FS] Chunked read: Success\n");
    }
    
    return error;
}

fs_error_t fs_write_file(const char *path, const uint8_t *data, size_t size) {
    if (!path || !data) {
        return FS_ERR_INVALID_PATH;
    }
    
    if (!fat32_is_mounted()) {
        return FS_ERR_NOT_MOUNTED;
    }
    
    /* Check size limit */
    if (size > FILE_SERVER_MAX_FILE_SIZE) {
        return FS_ERR_TOO_LARGE;
    }
    
    /* Delete existing file if it exists (to allow overwriting) */
    fat32_delete(path);  /* Ignore error - file might not exist */
    
    /* Create new file */
    fat32_file_t file;
    fat32_error_t result = fat32_create(&file, path);
    if (result != FAT32_OK) {
        return translate_fat32_error(result);
    }
    
    /* Write data */
    size_t bytes_written;
    result = fat32_write(&file, data, size, &bytes_written);
    fat32_close(&file);
    
    if (result != FAT32_OK) {
        return translate_fat32_error(result);
    }
    
    if (bytes_written != size) {
        return FS_ERR_IO;
    }
    
    return FS_OK;
}

fs_error_t fs_delete(const char *path) {
    if (!path) {
        return FS_ERR_INVALID_PATH;
    }
    
    if (!fat32_is_mounted()) {
        return FS_ERR_NOT_MOUNTED;
    }
    
    fat32_error_t result = fat32_delete(path);
    return translate_fat32_error(result);
}

fs_error_t fs_mkdir(const char *path) {
    if (!path) {
        return FS_ERR_INVALID_PATH;
    }
    
    if (!fat32_is_mounted()) {
        return FS_ERR_NOT_MOUNTED;
    }
    
    fat32_file_t dir;
    fat32_error_t result = fat32_dir_create(&dir, path);
    if (result == FAT32_OK) {
        fat32_close(&dir);
    }
    return translate_fat32_error(result);
}

fs_error_t fs_stat(const char *path, char **json_out) {
    if (!path || !json_out) {
        return FS_ERR_INVALID_PATH;
    }
    
    if (!fat32_is_mounted()) {
        return FS_ERR_NOT_MOUNTED;
    }
    
    /* Open file/directory */
    fat32_file_t file;
    fat32_error_t result = fat32_open(&file, path);
    if (result != FAT32_OK) {
        return translate_fat32_error(result);
    }
    
    /* Get file info */
    bool is_dir = (file.attributes & FAT32_ATTR_DIRECTORY) != 0;
    uint32_t size = is_dir ? 0 : fat32_size(&file);
    
    fat32_close(&file);
    
    /* Format as JSON */
    char *json = malloc(512);
    if (!json) {
        return FS_ERR_NO_MEMORY;
    }
    
    /* Extract filename from path */
    const char *filename = strrchr(path, '/');
    if (filename) {
        filename++;
    } else {
        filename = path;
    }
    
    char escaped_name[300];
    json_escape_string(filename, escaped_name, sizeof(escaped_name));
    
    snprintf(json, 512,
            "{\"name\":\"%s\",\"size\":%lu,\"is_dir\":%s}",
            escaped_name,
            (unsigned long)size,
            is_dir ? "true" : "false");
    
    *json_out = json;
    return FS_OK;
}