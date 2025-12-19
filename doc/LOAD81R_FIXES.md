# LOAD81R Bug Fixes and Implementation Notes

## Build 23 - Initial Implementation

### Issues Fixed

1. **Python Client Initialization Bug**
   - **Problem**: `Load81Client.__init__()` didn't accept host/port parameters
   - **Fix**: Added optional `host` and `port` parameters to `__init__()` method
   - **File**: `tools/load81r/client.py:25`

2. **Disconnect Method Mismatch**
   - **Problem**: Shell called `disconnect()` but client had `close()`
   - **Fix**: Changed shell to call `close()` instead
   - **File**: `tools/load81r/shell.py`

## Build 24 - Network Polling Fix

### Issues Fixed

3. **Network Polling in REPL Loop**
   - **Problem**: Server became unresponsive during REPL execution because network wasn't being polled
   - **Fix**: Added `cyw43_arch_poll()` call in REPL wait loop
   - **File**: `src/picocalc_repl.c:251-258`
   - **Details**: The REPL handler runs on Core 1 and waits for Core 0 to execute Lua code. During this wait, we must poll the network stack to keep the TCP connection alive and responsive.

## Build 25 - EDIT Command Debug

### Issues Fixed

4. **EDIT Command Upload Failure - File Overwrite Issue** (Build 25)
   - **Problem**: EDIT command downloads file successfully but upload fails with "File or directory already exists"
   - **Root Cause**: `fs_write_file()` called `fat32_create()` which fails if file already exists
   - **Fix**: Delete existing file before creating new one in `fs_write_file()`
   - **Files Modified**:
     - `src/picocalc_fs_handler.c:307` - Added `fat32_delete(path)` before `fat32_create()`
     - `tools/load81r/client.py:252-270` - Added debug output to `put()` method (for diagnostics)
     - `tools/load81r/client.py:7` - Added `sys` import for stderr output
   
   **Diagnostic Output Added**:
   - "DEBUG: PUT command failed: {error}" - If initial PUT command fails
   - "DEBUG: Expected READY, got: {data}" - If server doesn't respond with READY
   - "DEBUG: Failed to send data" - If binary data transmission fails
   - "DEBUG: Upload confirmation failed: {error}" - If final confirmation fails

## Build 26 - RSYNC and Disconnect Fixes

### Issues Fixed

5. **CLI Disconnect Error**
   - **Problem**: `load81r.py` called `client.disconnect()` but method is named `close()`
   - **Fix**: Changed line 132 to call `client.close()`
   - **File**: `tools/load81r/load81r.py:132`

6. **RSYNC Path Ambiguity**
   - **Problem**: `rsync load81 load81` was ambiguous - both paths looked local
   - **Root Cause**: Original logic only checked if path starts with `/` to determine if remote
   - **Fix**: Added validation to require at least one path to be absolute (start with `/`)
   - **Files Modified**:
     - `tools/load81r/commands.py:324-358` - Enhanced path validation and error messages
     - `tools/load81r/commands.py:197` - Updated help text
     - `tools/load81r/load81r.py:29` - Added rsync examples to CLI help
   
   **New Behavior**:
   - Remote paths MUST start with `/` (e.g., `/load81`)
   - Local paths should NOT start with `/` (e.g., `./backup` or `backup`)
   - Error if both paths are remote or both are local
   - Clear error messages guide correct usage

   **Correct Usage**:
   ```bash
   # Download from remote
   load81r.py 192.168.1.100 rsync /load81 ./backup
   
   # Upload to remote
   load81r.py 192.168.1.100 rsync ./backup /load81
   ```

### Testing Instructions

To test the EDIT command with debug output:

```bash
cd tools/load81r
python3 load81r.py <picocalc-ip>

# In the shell:
load81r> edit /load81/test.txt
```

The debug output will show exactly where the upload is failing:
- If you see "DEBUG: PUT command failed" - The server rejected the PUT command
- If you see "DEBUG: Expected READY" - The server sent wrong response
- If you see "DEBUG: Failed to send data" - Network transmission failed
- If you see "DEBUG: Upload confirmation failed" - Server couldn't write the file

### Possible Root Causes

Based on code analysis, potential issues:

1. **Path Normalization**: The filename might not be getting normalized correctly relative to current directory
2. **Binary Data Reception**: Server might not be receiving all the data before trying to write
3. **File System Error**: SD card write might be failing (permissions, space, etc.)
4. **Network Timing**: Data might be arriving in multiple packets and not being assembled correctly

### Server-Side PUT Flow

The server handles PUT in two phases:

**Phase 1 - Command Reception** ([`src/picocalc_file_server.c:271-315`](src/picocalc_file_server.c:271-315)):
1. Parse `PUT path size` command
2. Validate size (max 1MB)
3. Normalize path relative to current directory
4. Allocate buffer for incoming data
5. Set `receiving_data = true` flag
6. Send `+READY\n` response

**Phase 2 - Data Reception** ([`src/picocalc_file_server.c:468-500`](src/picocalc_file_server.c:468-500)):
1. Receive binary data in chunks via `file_recv()` callback
2. Copy data to buffer using `pbuf_copy_partial()`
3. Track bytes received vs expected
4. When all data received, call `fs_write_file()`
5. Send `+OK\n` or `-ERR` response

### Client-Side PUT Flow

The client sends PUT in three steps ([`tools/load81r/client.py:252-270`](tools/load81r/client.py:252-270)):

1. Send `PUT path size\n` command
2. Wait for `+READY\n` response
3. Send binary data (all at once via `sendall()`)
4. Wait for `+OK\n` or `-ERR` response

## WiFi Configuration

**Note**: WiFi credentials are configured in `/load81/start.lua` on the SD card, not in firmware. The file server starts automatically after successful WiFi connection.

Example `/load81/start.lua`:
```lua
wifi.connect("YourSSID", "YourPassword")
```

## Network Architecture

- **Port**: 1900 (TCP)
- **Protocol**: Text-based with binary data support
- **Concurrency**: Single client only (for SD card safety)
- **Core Assignment**: File server runs on Core 1, isolated from Lua interpreter on Core 0
- **Synchronization**: REPL commands use inter-core FIFO for Lua execution

## Build 26 - Chunked File Reading

### Issues Fixed

7. **Large File Memory Allocation Failure**
   - **Problem**: RSYNC or CAT fails on larger files (>10KB) with "Cannot read" error due to memory fragmentation
   - **Example**: `Error: Cannot read /load81/scorched.lua` (13KB file)
   - **Root Cause**: Original implementation allocated entire file into RAM before sending
   - **Fix**: Implemented chunked file reading with 4KB buffer
   - **Files Modified**:
     - `src/picocalc_fs_handler.h:79-88` - Added `fs_read_file_chunked()` API with callback
     - `src/picocalc_fs_handler.c:291-363` - Implemented chunked reading with 4KB buffer
     - `src/picocalc_file_server.c:242-313` - Modified CAT command to use chunked reading for files >8KB
   
   **Implementation Details**:
   - Small files (<8KB) use original method for efficiency
   - Large files (≥8KB) use chunked reading with callback
   - Only allocates 4KB buffer regardless of file size
   - Streams data directly to TCP without intermediate buffering
   
   **Benefits**:
   - Eliminates memory fragmentation issues
   - Supports files up to 1MB limit
   - Minimal memory footprint (4KB buffer)
   - No protocol changes required

## Build 27 - CAT Command Double-Read Fix

### Issues Fixed

8. **CAT Command Double-Read Causing WiFi Initialization Issues**
   - **Problem**: After implementing chunked file reading in Build 26, splash screen stayed on much longer and WiFi no longer connected properly
   - **Symptom**: System appeared to hang during initialization
   - **Root Cause**: The `cmd_cat()` function was inefficiently reading files twice:
     1. Called `fs_read_file()` to get file size - allocated and read **entire file** into memory
     2. For large files (≥8KB), freed that buffer and read file again using chunked reading
   - **Example**: For a 13KB file like `scorched.lua`:
     - Allocate 13KB buffer during startup
     - Read entire 13KB file
     - Free 13KB buffer
     - Allocate 4KB chunk buffer
     - Read 13KB file again in chunks
   - **Impact**: Double-read and large memory allocation during system initialization caused memory fragmentation that interfered with WiFi initialization
   - **Fix**: Added lightweight `fs_get_file_size()` function and modified `cmd_cat()` to check size before reading
   - **Files Modified**:
     - `src/picocalc_fs_handler.h:78-88` - Added `fs_get_file_size()` declaration
     - `src/picocalc_fs_handler.c:307-350` - Implemented `fs_get_file_size()` function
     - `src/picocalc_file_server.c:275-322` - Modified `cmd_cat()` to use size check before reading
   
   **New Implementation**:
   - `fs_get_file_size()` opens file, gets size using `fat32_size()` (no data read), closes file
   - `cmd_cat()` now:
     1. Calls `fs_get_file_size()` first (lightweight, no memory allocation)
     2. For small files (<8KB): reads entire file once with `fs_read_file()`
     3. For large files (≥8KB): uses chunked reading with `fs_read_file_chunked()`
   
   **Benefits**:
   - Eliminates wasteful double-read of files
   - Reduces memory fragmentation during startup
   - Restores normal WiFi initialization timing
   - Maintains efficient handling of both small and large files

## Build 28 - Removed Automatic File Server Startup

### Issues Fixed

9. **WiFi Connection Delay/Failure Due to File Server Initialization**
   - **Problem**: After Build 26, splash screen stayed on much longer and WiFi failed to connect
   - **Root Cause**: File server initialization (`file_server_init()` and `file_server_start()`) was being called synchronously during WiFi connection in `lua_wifi_connect()`, blocking the Lua script execution and potentially interfering with WiFi initialization
   - **Impact**: WiFi connection would timeout (30 seconds), causing long splash screen delays
   - **Fix**: Removed automatic file server startup from WiFi connection path
   - **Files Modified**:
     - `src/picocalc_wifi.c:134-147` - Removed `file_server_init()` and `file_server_start()` calls
     - `src/picocalc_wifi.c:182-194` - Removed `file_server_stop()` call from disconnect
   
   **New Behavior**:
   - WiFi connects normally without file server interference
   - File server must be started manually if needed (future enhancement)
   - This restores normal WiFi connection timing
   
   **Temporary Limitation**:
   - LOAD81R file server is currently disabled until a proper initialization mechanism is implemented
   - WiFi functionality is fully restored
   - Future builds will add a non-blocking file server startup mechanism

## Known Limitations

1. **Single Client**: Only one client can connect at a time
2. **File Size**: Maximum file size is 1MB (configurable via `FILE_SERVER_MAX_FILE_SIZE`)
3. **No Authentication**: Protocol has no authentication (suitable for trusted networks only)
4. **No Encryption**: All data transmitted in plaintext
5. **No Resume**: File transfers cannot be resumed if interrupted

## Performance Notes

- File transfers are limited by WiFi bandwidth (~1-2 MB/s typical)
- Large directory listings may take time to generate JSON
- REPL commands block until Lua execution completes
- Network polling in REPL loop adds ~10ms latency per command