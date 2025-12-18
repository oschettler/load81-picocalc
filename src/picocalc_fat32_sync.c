/**
 * @file picocalc_fat32_sync.c
 * @brief Thread-Safe FAT32 Access Layer Implementation
 * 
 * Provides mutex-protected wrappers around FAT32 operations for safe
 * concurrent access from multiple cores.
 */

#include "picocalc_fat32_sync.h"
#include "pico/sync.h"
#include <string.h>

/* Global mutex for FAT32 access */
static mutex_t fat32_mutex;
static bool fat32_sync_initialized = false;

/* Timeout for FAT32 operations (in milliseconds) */
#define FAT32_SYNC_DEFAULT_TIMEOUT_MS 5000

/**
 * @brief Initialize FAT32 synchronization layer
 */
void fat32_sync_init(void) {
    if (!fat32_sync_initialized) {
        mutex_init(&fat32_mutex);
        fat32_sync_initialized = true;
    }
}

/**
 * @brief Manually acquire FAT32 mutex
 */
bool fat32_sync_lock(uint32_t timeout_ms) {
    if (!fat32_sync_initialized) {
        return false;
    }
    
    if (timeout_ms == 0) {
        return mutex_try_enter(&fat32_mutex, NULL);
    } else if (timeout_ms == UINT32_MAX) {
        mutex_enter_blocking(&fat32_mutex);
        return true;
    } else {
        absolute_time_t timeout = make_timeout_time_ms(timeout_ms);
        return mutex_enter_block_until(&fat32_mutex, timeout);
    }
}

/**
 * @brief Release FAT32 mutex
 */
void fat32_sync_unlock(void) {
    if (fat32_sync_initialized) {
        mutex_exit(&fat32_mutex);
    }
}

/* ========================================================================
 * Thread-Safe FAT32 File Operations
 * ======================================================================== */

fat32_error_t fat32_sync_open(fat32_file_t *file, const char *path) {
    if (!fat32_sync_lock(FAT32_SYNC_DEFAULT_TIMEOUT_MS)) {
        return FAT32_ERROR_INIT_FAILED;
    }
    
    fat32_error_t result = fat32_open(file, path);
    fat32_sync_unlock();
    return result;
}

fat32_error_t fat32_sync_create(fat32_file_t *file, const char *path) {
    if (!fat32_sync_lock(FAT32_SYNC_DEFAULT_TIMEOUT_MS)) {
        return FAT32_ERROR_INIT_FAILED;
    }
    
    fat32_error_t result = fat32_create(file, path);
    fat32_sync_unlock();
    return result;
}

fat32_error_t fat32_sync_close(fat32_file_t *file) {
    if (!fat32_sync_lock(FAT32_SYNC_DEFAULT_TIMEOUT_MS)) {
        return FAT32_ERROR_INIT_FAILED;
    }
    
    fat32_error_t result = fat32_close(file);
    fat32_sync_unlock();
    return result;
}

fat32_error_t fat32_sync_read(fat32_file_t *file, void *buffer, size_t size, 
                               size_t *bytes_read) {
    if (!fat32_sync_lock(FAT32_SYNC_DEFAULT_TIMEOUT_MS)) {
        return FAT32_ERROR_INIT_FAILED;
    }
    
    fat32_error_t result = fat32_read(file, buffer, size, bytes_read);
    fat32_sync_unlock();
    return result;
}

fat32_error_t fat32_sync_write(fat32_file_t *file, const void *buffer, 
                                size_t size, size_t *bytes_written) {
    if (!fat32_sync_lock(FAT32_SYNC_DEFAULT_TIMEOUT_MS)) {
        return FAT32_ERROR_INIT_FAILED;
    }
    
    fat32_error_t result = fat32_write(file, buffer, size, bytes_written);
    fat32_sync_unlock();
    return result;
}

fat32_error_t fat32_sync_seek(fat32_file_t *file, uint32_t position) {
    if (!fat32_sync_lock(FAT32_SYNC_DEFAULT_TIMEOUT_MS)) {
        return FAT32_ERROR_INIT_FAILED;
    }
    
    fat32_error_t result = fat32_seek(file, position);
    fat32_sync_unlock();
    return result;
}

uint32_t fat32_sync_tell(fat32_file_t *file) {
    if (!fat32_sync_lock(FAT32_SYNC_DEFAULT_TIMEOUT_MS)) {
        return 0;
    }
    
    uint32_t result = fat32_tell(file);
    fat32_sync_unlock();
    return result;
}

uint32_t fat32_sync_size(fat32_file_t *file) {
    if (!fat32_sync_lock(FAT32_SYNC_DEFAULT_TIMEOUT_MS)) {
        return 0;
    }
    
    uint32_t result = fat32_size(file);
    fat32_sync_unlock();
    return result;
}

bool fat32_sync_eof(fat32_file_t *file) {
    if (!fat32_sync_lock(FAT32_SYNC_DEFAULT_TIMEOUT_MS)) {
        return true;
    }
    
    bool result = fat32_eof(file);
    fat32_sync_unlock();
    return result;
}

fat32_error_t fat32_sync_delete(const char *path) {
    if (!fat32_sync_lock(FAT32_SYNC_DEFAULT_TIMEOUT_MS)) {
        return FAT32_ERROR_INIT_FAILED;
    }
    
    fat32_error_t result = fat32_delete(path);
    fat32_sync_unlock();
    return result;
}

fat32_error_t fat32_sync_rename(const char *old_path, const char *new_path) {
    if (!fat32_sync_lock(FAT32_SYNC_DEFAULT_TIMEOUT_MS)) {
        return FAT32_ERROR_INIT_FAILED;
    }
    
    fat32_error_t result = fat32_rename(old_path, new_path);
    fat32_sync_unlock();
    return result;
}

/* ========================================================================
 * Thread-Safe FAT32 Directory Operations
 * ======================================================================== */

fat32_error_t fat32_sync_dir_read(fat32_file_t *dir, fat32_entry_t *entry) {
    if (!fat32_sync_lock(FAT32_SYNC_DEFAULT_TIMEOUT_MS)) {
        return FAT32_ERROR_INIT_FAILED;
    }
    
    fat32_error_t result = fat32_dir_read(dir, entry);
    fat32_sync_unlock();
    return result;
}

fat32_error_t fat32_sync_dir_create(fat32_file_t *dir, const char *path) {
    if (!fat32_sync_lock(FAT32_SYNC_DEFAULT_TIMEOUT_MS)) {
        return FAT32_ERROR_INIT_FAILED;
    }
    
    fat32_error_t result = fat32_dir_create(dir, path);
    fat32_sync_unlock();
    return result;
}

fat32_error_t fat32_sync_set_current_dir(const char *path) {
    if (!fat32_sync_lock(FAT32_SYNC_DEFAULT_TIMEOUT_MS)) {
        return FAT32_ERROR_INIT_FAILED;
    }
    
    fat32_error_t result = fat32_set_current_dir(path);
    fat32_sync_unlock();
    return result;
}

fat32_error_t fat32_sync_get_current_dir(char *path, size_t path_len) {
    if (!fat32_sync_lock(FAT32_SYNC_DEFAULT_TIMEOUT_MS)) {
        return FAT32_ERROR_INIT_FAILED;
    }
    
    fat32_error_t result = fat32_get_current_dir(path, path_len);
    fat32_sync_unlock();
    return result;
}

/* ========================================================================
 * Thread-Safe FAT32 Filesystem Operations
 * ======================================================================== */

bool fat32_sync_is_ready(void) {
    if (!fat32_sync_lock(FAT32_SYNC_DEFAULT_TIMEOUT_MS)) {
        return false;
    }
    
    bool result = fat32_is_ready();
    fat32_sync_unlock();
    return result;
}

fat32_error_t fat32_sync_get_status(void) {
    if (!fat32_sync_lock(FAT32_SYNC_DEFAULT_TIMEOUT_MS)) {
        return FAT32_ERROR_INIT_FAILED;
    }
    
    fat32_error_t result = fat32_get_status();
    fat32_sync_unlock();
    return result;
}

fat32_error_t fat32_sync_get_free_space(uint64_t *free_space) {
    if (!fat32_sync_lock(FAT32_SYNC_DEFAULT_TIMEOUT_MS)) {
        return FAT32_ERROR_INIT_FAILED;
    }
    
    fat32_error_t result = fat32_get_free_space(free_space);
    fat32_sync_unlock();
    return result;
}

fat32_error_t fat32_sync_get_total_space(uint64_t *total_space) {
    if (!fat32_sync_lock(FAT32_SYNC_DEFAULT_TIMEOUT_MS)) {
        return FAT32_ERROR_INIT_FAILED;
    }
    
    fat32_error_t result = fat32_get_total_space(total_space);
    fat32_sync_unlock();
    return result;
}

fat32_error_t fat32_sync_get_volume_name(char *name, size_t name_len) {
    if (!fat32_sync_lock(FAT32_SYNC_DEFAULT_TIMEOUT_MS)) {
        return FAT32_ERROR_INIT_FAILED;
    }
    
    fat32_error_t result = fat32_get_volume_name(name, name_len);
    fat32_sync_unlock();
    return result;
}

uint32_t fat32_sync_get_cluster_size(void) {
    if (!fat32_sync_lock(FAT32_SYNC_DEFAULT_TIMEOUT_MS)) {
        return 0;
    }
    
    uint32_t result = fat32_get_cluster_size();
    fat32_sync_unlock();
    return result;
}