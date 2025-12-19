#ifndef PICOCALC_FS_HANDLER_H
#define PICOCALC_FS_HANDLER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @file picocalc_fs_handler.h
 * @brief File system operations handler for LOAD81R server
 * 
 * Provides high-level file system operations that wrap the FAT32 driver
 * with path normalization, JSON formatting, and error handling.
 */

/* Error codes */
typedef enum {
    FS_OK = 0,
    FS_ERR_NOT_FOUND,
    FS_ERR_NOT_DIR,
    FS_ERR_NOT_FILE,
    FS_ERR_EXISTS,
    FS_ERR_NO_SPACE,
    FS_ERR_IO,
    FS_ERR_INVALID_PATH,
    FS_ERR_NO_MEMORY,
    FS_ERR_TOO_LARGE,
    FS_ERR_NOT_MOUNTED
} fs_error_t;

/* File/directory entry information */
typedef struct {
    char name[256];
    uint32_t size;
    bool is_dir;
    uint16_t date;
    uint16_t time;
} fs_entry_t;

/**
 * Initialize file system handler
 * 
 * @return FS_OK on success, error code otherwise
 */
fs_error_t fs_init(void);

/**
 * Normalize a path relative to current working directory
 * Handles ., .., absolute and relative paths
 * 
 * @param path Input path
 * @param cwd Current working directory
 * @param out Output buffer for normalized path
 * @param out_len Size of output buffer
 * @return FS_OK on success, error code otherwise
 */
fs_error_t fs_normalize_path(const char *path, const char *cwd, 
                             char *out, size_t out_len);

/**
 * List directory contents
 * Returns JSON array of entries
 * 
 * @param path Directory path
 * @param json_out Output: allocated JSON string (caller must free)
 * @return FS_OK on success, error code otherwise
 */
fs_error_t fs_list_dir(const char *path, char **json_out);

/**
 * Read entire file into memory
 *
 * @param path File path
 * @param data Output: allocated data buffer (caller must free)
 * @param size Output: size of data
 * @return FS_OK on success, error code otherwise
 */
fs_error_t fs_read_file(const char *path, uint8_t **data, size_t *size);

/**
 * Get file size without reading data
 * Lightweight operation that just opens file and returns size
 *
 * @param path File path
 * @param size Output: file size in bytes
 * @return FS_OK on success, error code otherwise
 */
fs_error_t fs_get_file_size(const char *path, size_t *size);

/**
 * Read file in chunks using a callback
 * Avoids allocating large buffers by streaming data
 *
 * @param path File path
 * @param callback Function called for each chunk (return false to abort)
 * @param user_data User data passed to callback
 * @return FS_OK on success, error code otherwise
 */
typedef bool (*fs_read_chunk_callback_t)(const uint8_t *chunk, size_t size, void *user_data);
fs_error_t fs_read_file_chunked(const char *path, fs_read_chunk_callback_t callback, void *user_data);

/**
 * Write data to file (creates or overwrites)
 * 
 * @param path File path
 * @param data Data to write
 * @param size Size of data
 * @return FS_OK on success, error code otherwise
 */
fs_error_t fs_write_file(const char *path, const uint8_t *data, size_t size);

/**
 * Delete file or empty directory
 * 
 * @param path Path to delete
 * @return FS_OK on success, error code otherwise
 */
fs_error_t fs_delete(const char *path);

/**
 * Create directory
 * 
 * @param path Directory path
 * @return FS_OK on success, error code otherwise
 */
fs_error_t fs_mkdir(const char *path);

/**
 * Get file/directory information
 * Returns JSON object with file info
 * 
 * @param path File/directory path
 * @param json_out Output: allocated JSON string (caller must free)
 * @return FS_OK on success, error code otherwise
 */
fs_error_t fs_stat(const char *path, char **json_out);

/**
 * Get error message string
 * 
 * @param error Error code
 * @return Human-readable error message
 */
const char *fs_error_string(fs_error_t error);

#endif /* PICOCALC_FS_HANDLER_H */