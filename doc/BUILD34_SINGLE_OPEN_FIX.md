# Build 34: Single File Open Fix for Large File Reading

## Problem Summary

**Issue**: Files ≥8KB fail to read via CAT and RSYNC commands
- Files < 8KB work correctly
- Files ≥ 8KB (like `scorched.lua` at 12.2K) fail with "Cannot read" error
- After first failure, ALL subsequent files fail in RSYNC operations

**Root Cause**: Double file open pattern in `cmd_cat()`:
1. First open in `fs_get_file_size()` to check size
2. Close file
3. Second open in `fs_read_file_chunked()` to read data
4. This rapid open/close/open cycle appears to cause issues with FAT32 filesystem layer

## Solution Implemented

### Eliminated Double File Open

**Before (Build 33)**:
```c
// cmd_cat() in picocalc_file_server.c
int32_t file_size = fs_get_file_size(path);  // First open
if (file_size < 0) {
    send_error(pcb, "Cannot read file");
    return;
}

if (file_size <= 8192) {
    // Use fs_read_file() - third open!
} else {
    // Use fs_read_file_chunked() - second open
}
```

**After (Build 34)**:
```c
// cmd_cat() in picocalc_file_server.c
// Single open via fs_read_file_chunked() for ALL files
cat_chunk_context_t ctx = {
    .pcb = pcb,
    .file_size = 0,  // Will be set by chunked reader
    .bytes_sent = 0,
    .error_occurred = false
};

fs_error_t result = fs_read_file_chunked(path, cat_chunk_callback, &ctx);
```

### Modified Chunked Reader to Provide File Size

**Before (Build 33)**:
```c
// fs_read_file_chunked() didn't provide file size to callback
```

**After (Build 34)**:
```c
// fs_read_file_chunked() in picocalc_fs_handler.c
typedef struct {
    void *user_data;
    int32_t file_size;  // NEW: Provide size to callback
} chunk_callback_wrapper_t;

// Set file size before first callback
wrapper.file_size = (int32_t)file_size;

// Callback receives file size
typedef bool (*fs_chunk_callback_t)(const uint8_t *data, size_t len, 
                                    int32_t file_size, void *user_data);
```

### Improved Error Handling

**Protocol Corruption Prevention**:
```c
// If error occurs AFTER sending +DATA header, close connection
if (ctx.error_occurred) {
    DEBUG_PRINTF("CAT: Error during chunked read, closing connection\n");
    tcp_close(pcb);
    return;
}
```

**Why**: The Python client expects binary data after `+DATA` header. Sending `-ERR` at this point corrupts the protocol stream, causing all subsequent commands to fail.

## Changes Made

### File: `src/picocalc_file_server.c`

**Lines 243-275**: Modified `cat_chunk_context_t` and `cat_chunk_callback()`
- Added `file_size` field to context
- Callback now receives file size from chunked reader
- Sends `+DATA <size>` header on first chunk
- Tracks bytes sent and detects errors

**Lines 277-320**: Simplified `cmd_cat()`
- Removed `fs_get_file_size()` call (no more double open)
- Always uses `fs_read_file_chunked()` for all files
- Proper error handling: closes connection if error occurs after `+DATA` header
- No more size-based branching between `fs_read_file()` and `fs_read_file_chunked()`

### File: `src/picocalc_fs_handler.c`

**Lines 344-423**: Modified `fs_read_file_chunked()`
- Added `chunk_callback_wrapper_t` to pass file size to callback
- Callback signature changed to include `int32_t file_size` parameter
- Sets file size before first callback invocation
- Returns `FS_ERR_IO` if callback returns false (abort)
- Extensive DEBUG_PRINTF logging for troubleshooting

**Lines 309-342**: `fs_get_file_size()` still exists
- Used by other commands (LS, RSYNC directory scanning)
- No longer used by CAT command

## Testing Instructions

### 1. Flash Build 34

```bash
cp build/load81_picocalc.uf2 /media/RPI-RP2/
```

### 2. Connect to WiFi

Power on PicoCalc, wait for WiFi connection. Check IP address on display.

### 3. Test Small Files (< 8KB)

```bash
# Should work (baseline test)
tools/load81r/load81r.py <picocalc-ip> cat /load81/hello.lua
```

### 4. Test Large Files (≥ 8KB)

```bash
# This was failing in Build 33
tools/load81r/load81r.py <picocalc-ip> cat /load81/scorched.lua

# Expected: Full file content displayed
# If fails: Check for "Cannot read" error
```

### 5. Test RSYNC with Multiple Files

```bash
# This was failing after first large file in Build 33
tools/load81r/load81r.py <picocalc-ip> rsync /load81 ./backup

# Expected: All files downloaded successfully
# If fails: Check which file causes first failure
```

### 6. Test Edge Cases

```bash
# Test file exactly at 8KB boundary
tools/load81r/load81r.py <picocalc-ip> cat /load81/8kb_file.lua

# Test very large file (if available)
tools/load81r/load81r.py <picocalc-ip> cat /load81/large_file.lua
```

## Debug Output

**Note**: USB debug output is currently not working. User has connected:
- `/dev/ttyACM0` - Micro USB on Pico2W board (shows "Connected" but no messages)
- `/dev/ttyUSB0` - USB-C on PicoCalc (shows "Connected" but no messages)

Debug output may require:
1. Correct USB device selection
2. Proper baud rate configuration
3. USB CDC initialization timing
4. Alternative debug method (network logging, LED patterns)

## Expected Behavior

### Success Indicators

1. **Small files**: Continue to work as before
2. **Large files**: Now read successfully without "Cannot read" error
3. **RSYNC**: All files download without cascading failures
4. **Protocol**: No corruption, all commands work after large file operations

### Failure Indicators

1. **Still fails on large files**: May indicate deeper FAT32 issue
2. **Fails on all files**: Regression in chunked reader
3. **Protocol corruption**: Error handling not working correctly

## Alternative Approaches (If Still Failing)

### Option 1: Revert to Non-Chunked Reading

```c
// Always use fs_read_file() for all files
// Accept memory limitation for very large files
```

### Option 2: Add Delay Between Operations

```c
// Add small delay after file close
sleep_ms(10);
```

### Option 3: Investigate FAT32 Layer

```c
// Check if file handles are being reused incorrectly
// Verify f_close() is properly releasing resources
```

### Option 4: Implement File Handle Caching

```c
// Keep file open between size check and read
// Pass open file handle to chunked reader
```

## Technical Details

### Memory Usage

- **Chunk size**: 4KB (4096 bytes)
- **Stack usage**: Minimal (context structure only)
- **Heap usage**: None (stack-allocated buffers)

### Protocol Flow

```
Client: CAT /load81/scorched.lua
Server: +DATA 12345
Server: <4096 bytes binary data>
Server: <4096 bytes binary data>
Server: <4153 bytes binary data>
Server: +END
```

### Error Flow (If Read Fails)

```
Client: CAT /load81/scorched.lua
Server: +DATA 12345
Server: <connection closed>  # No -ERR sent after +DATA
```

## Build Information

- **Build Number**: 34
- **Date**: 2025-12-19
- **Compiler**: arm-none-eabi-gcc 13.2.1
- **Firmware Size**: (check build output)
- **Memory Usage**: (check build output)

## Related Documents

- `doc/BUILD32_LARGE_FILE_FIX.md` - Previous attempt (protocol corruption fix)
- `doc/BUILD33_DOUBLE_OPEN_DEBUG.md` - Analysis of double open issue
- `doc/LOAD81R_TESTING.md` - Comprehensive testing guide
- `plans/load81r_architecture.md` - System architecture

## Next Steps

1. Flash Build 34 to hardware
2. Test large file reading with CAT command
3. Test RSYNC with directory containing large files
4. If successful: Mark task complete
5. If still failing: Investigate FAT32 layer or try alternative approaches