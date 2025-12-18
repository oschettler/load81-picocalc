/**
 * @file picocalc_fat32_sync.h
 * @brief Thread-Safe FAT32 Access Layer
 * 
 * Provides mutex-protected wrappers around FAT32 operations for safe
 * concurrent access from multiple cores (Core 0: Lua/UI, Core 1: 9P server).
 */

#ifndef PICOCALC_FAT32_SYNC_H
#define PICOCALC_FAT32_SYNC_H

#include "fat32.h"
#include "pico/mutex.h"
#include <stdbool.h>

/**
 * @brief Initialize FAT32 synchronization layer
 * 
 * Must be called before any other fat32_sync_* functions.
 * Should be called from Core 0 during system initialization.
 */
void fat32_sync_init(void);

/**
 * @brief Manually acquire FAT32 mutex
 * @param timeout_ms Timeout in milliseconds (0 = no wait, UINT32_MAX = infinite)
 * @return true if lock acquired, false on timeout
 * 
 * Use this for batch operations to avoid repeated lock/unlock overhead.
 * Must call fat32_sync_unlock() when done.
 */
bool fat32_sync_lock(uint32_t timeout_ms);

/**
 * @brief Release FAT32 mutex
 * 
 * Must be called after fat32_sync_lock() to release the lock.
 */
void fat32_sync_unlock(void);

/* ========================================================================
 * Thread-Safe FAT32 File Operations
 * ======================================================================== */

/**
 * @brief Open file (thread-safe)
 * @param file File handle to populate
 * @param path File path
 * @return FAT32 error code
 */
fat32_error_t fat32_sync_open(fat32_file_t *file, const char *path);

/**
 * @brief Create new file (thread-safe)
 * @param file File handle to populate
 * @param path File path
 * @return FAT32 error code
 */
fat32_error_t fat32_sync_create(fat32_file_t *file, const char *path);

/**
 * @brief Close file (thread-safe)
 * @param file File handle
 * @return FAT32 error code
 */
fat32_error_t fat32_sync_close(fat32_file_t *file);

/**
 * @brief Read from file (thread-safe)
 * @param file File handle
 * @param buffer Destination buffer
 * @param size Number of bytes to read
 * @param bytes_read Pointer to store actual bytes read (can be NULL)
 * @return FAT32 error code
 */
fat32_error_t fat32_sync_read(fat32_file_t *file, void *buffer, size_t size, 
                               size_t *bytes_read);

/**
 * @brief Write to file (thread-safe)
 * @param file File handle
 * @param buffer Source buffer
 * @param size Number of bytes to write
 * @param bytes_written Pointer to store actual bytes written (can be NULL)
 * @return FAT32 error code
 */
fat32_error_t fat32_sync_write(fat32_file_t *file, const void *buffer, 
                                size_t size, size_t *bytes_written);

/**
 * @brief Seek to position in file (thread-safe)
 * @param file File handle
 * @param position New file position
 * @return FAT32 error code
 */
fat32_error_t fat32_sync_seek(fat32_file_t *file, uint32_t position);

/**
 * @brief Get current file position (thread-safe)
 * @param file File handle
 * @return Current position, or 0 on error
 */
uint32_t fat32_sync_tell(fat32_file_t *file);

/**
 * @brief Get file size (thread-safe)
 * @param file File handle
 * @return File size in bytes, or 0 on error
 */
uint32_t fat32_sync_size(fat32_file_t *file);

/**
 * @brief Check if at end of file (thread-safe)
 * @param file File handle
 * @return true if at EOF
 */
bool fat32_sync_eof(fat32_file_t *file);

/**
 * @brief Delete file or directory (thread-safe)
 * @param path Path to delete
 * @return FAT32 error code
 */
fat32_error_t fat32_sync_delete(const char *path);

/**
 * @brief Rename/move file or directory (thread-safe)
 * @param old_path Current path
 * @param new_path New path
 * @return FAT32 error code
 */
fat32_error_t fat32_sync_rename(const char *old_path, const char *new_path);

/* ========================================================================
 * Thread-Safe FAT32 Directory Operations
 * ======================================================================== */

/**
 * @brief Read directory entry (thread-safe)
 * @param dir Directory handle
 * @param entry Entry structure to populate
 * @return FAT32 error code
 */
fat32_error_t fat32_sync_dir_read(fat32_file_t *dir, fat32_entry_t *entry);

/**
 * @brief Create directory (thread-safe)
 * @param dir Directory handle to populate
 * @param path Directory path
 * @return FAT32 error code
 */
fat32_error_t fat32_sync_dir_create(fat32_file_t *dir, const char *path);

/**
 * @brief Set current directory (thread-safe)
 * @param path Directory path
 * @return FAT32 error code
 */
fat32_error_t fat32_sync_set_current_dir(const char *path);

/**
 * @brief Get current directory (thread-safe)
 * @param path Buffer to store path
 * @param path_len Buffer size
 * @return FAT32 error code
 */
fat32_error_t fat32_sync_get_current_dir(char *path, size_t path_len);

/* ========================================================================
 * Thread-Safe FAT32 Filesystem Operations
 * ======================================================================== */

/**
 * @brief Check if filesystem is ready (thread-safe)
 * @return true if mounted and ready
 */
bool fat32_sync_is_ready(void);

/**
 * @brief Get filesystem status (thread-safe)
 * @return FAT32 error code
 */
fat32_error_t fat32_sync_get_status(void);

/**
 * @brief Get free space (thread-safe)
 * @param free_space Pointer to store free space in bytes
 * @return FAT32 error code
 */
fat32_error_t fat32_sync_get_free_space(uint64_t *free_space);

/**
 * @brief Get total space (thread-safe)
 * @param total_space Pointer to store total space in bytes
 * @return FAT32 error code
 */
fat32_error_t fat32_sync_get_total_space(uint64_t *total_space);

/**
 * @brief Get volume name (thread-safe)
 * @param name Buffer to store volume name
 * @param name_len Buffer size
 * @return FAT32 error code
 */
fat32_error_t fat32_sync_get_volume_name(char *name, size_t name_len);

/**
 * @brief Get cluster size (thread-safe)
 * @return Cluster size in bytes
 */
uint32_t fat32_sync_get_cluster_size(void);

#endif /* PICOCALC_FAT32_SYNC_H */