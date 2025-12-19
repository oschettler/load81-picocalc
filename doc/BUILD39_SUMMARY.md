# Build 39: Port Conflict Resolution - Summary

## Overview

**Build 39** resolves a critical port conflict that prevented debug log access, enabling diagnosis of the large file reading timeout issue discovered in Build 37.

## Problem Statement

### Primary Issue: Port Conflict
Both the load81r file server and diagnostic server were configured to use port 1900, causing:
- Diagnostic server unable to bind (port already in use)
- Debug log inaccessible
- Unable to diagnose large file timeout issue

### Secondary Issue: Large File Timeout
Files ≥8KB timeout after 30 seconds when reading via CAT or RSYNC commands.

## Root Cause Analysis

### Port Conflict Discovery
1. Build 38 implemented internal debug buffer system
2. Debug log retrieval showed empty output
3. Investigation revealed both servers on port 1900:
   - [`src/picocalc_file_server.h:19`](../src/picocalc_file_server.h:19): `FILE_SERVER_PORT 1900`
   - [`src/picocalc_diag_server.c:22`](../src/picocalc_diag_server.c:22): `DIAG_PORT 1900`

### Why This Matters
- Only one service can bind to a port at a time
- File server starts first (higher priority)
- Diagnostic server fails silently
- Debug logs never accessible despite being generated

## Solution Implemented

### Port Reassignment
Changed diagnostic server from port 1900 to **port 1901**:

**Modified Files:**
1. [`src/picocalc_diag_server.c:22`](../src/picocalc_diag_server.c:22)
   ```c
   #define DIAG_PORT 1901  // Changed from 1900
   ```

2. [`tools/get_debug_log.sh:6`](../tools/get_debug_log.sh:6)
   ```bash
   PORT=1901  # Changed from 1900
   ```

3. [`doc/BUILD38_DEBUG_OUTPUT.md`](../doc/BUILD38_DEBUG_OUTPUT.md)
   - Updated all references to port 1901
   - Added note about conflict avoidance

## Port Allocation Table

| Service | Port | Purpose | Status |
|---------|------|---------|--------|
| load81r File Server | 1900 | Remote shell, file operations, REPL | Active |
| Diagnostic Server | 1901 | Debug log access, system diagnostics | Active |

## Build Verification

### Compilation Status
✅ **Build Successful**
```
[100%] Built target load81_picocalc
```

### Memory Usage
```
   text     data      bss      dec      hex    filename
 555816        0   299312   855128    d0c58    build/load81_picocalc.elf
```

**Analysis:**
- BSS: 299,312 bytes (292 KB) = 56% of 520KB RAM ✅
- Text: 555,816 bytes (543 KB) = fits in flash ✅
- No memory concerns

## Testing Instructions

### 1. Flash Firmware
```bash
cd /home/olav/Dokumente/load81
picotool load -f build/load81_picocalc.uf2
```

### 2. Verify File Server (Port 1900)
```bash
echo "HELLO" | nc 192.168.178.122 1900
```
**Expected:** `+OK load81r/1.0`

### 3. Verify Diagnostic Server (Port 1901)
```bash
echo "status" | nc 192.168.178.122 1901
```
**Expected:** System status with debug log

### 4. Retrieve Debug Log
```bash
./tools/get_debug_log.sh 192.168.178.122
```

### 5. Test Large File Reading
```bash
# Using load81r client
./tools/load81r/load81r.py 192.168.178.122 cat /sd/large_file.lua

# Monitor debug log in separate terminal
watch -n 1 './tools/get_debug_log.sh 192.168.178.122'
```

## Expected Debug Output

With comprehensive logging added in Build 38, we should see:

### From [`fs_read_file()`](../src/picocalc_fs_handler.c:240-307):
```
[FS] Reading file: /sd/large_file.lua
[FS] File opened successfully
[FS] File size: 8192 bytes
[FS] Allocated buffer: 8192 bytes
[FS] Read chunk 1: 4096/8192 bytes
[FS] Read chunk 2: 8192/8192 bytes
[FS] File read complete
```

### From [`cmd_cat()`](../src/picocalc_file_server.c:289-316):
```
[CAT] Reading file: /sd/large_file.lua
[CAT] File read successful, size: 8192
[CAT] Sending data...
```

### From [`send_data()`](../src/picocalc_file_server.c:110-138):
```
[SEND] Sending +DATA header
[SEND] Sending 8192 bytes in chunks
[SEND] Chunk 1: 4096 bytes
[SEND] Chunk 2: 4096 bytes
[SEND] Sending +END marker
[SEND] Data transmission complete
```

## Diagnostic Strategy

### If Timeout Still Occurs:
1. **Check where it hangs:**
   - Before file open? → SD card issue
   - During read? → FAT32 driver issue
   - During send? → TCP/lwIP issue

2. **Analyze debug log patterns:**
   - Last successful operation before timeout
   - Any FAT32 error codes
   - TCP send buffer status

3. **Possible Root Causes:**
   - SD card read performance degradation
   - FAT32 driver blocking on large reads
   - lwIP send buffer exhaustion
   - TCP flow control issues

## Next Steps

1. ✅ Build firmware with port conflict resolved
2. ⏳ Flash to PicoCalc hardware
3. ⏳ Verify both servers accessible
4. ⏳ Retrieve debug log during large file read
5. ⏳ Analyze debug output to identify timeout cause
6. ⏳ Implement targeted fix based on findings

## Related Documentation

- [`doc/BUILD38_DEBUG_OUTPUT.md`](BUILD38_DEBUG_OUTPUT.md) - Debug system implementation
- [`doc/BUILD37_SIMPLE_READ.md`](BUILD37_SIMPLE_READ.md) - Simplified read approach
- [`doc/BUILD35_DETAILED_ERROR.md`](BUILD35_DETAILED_ERROR.md) - Error reporting enhancement
- [`doc/BUILD39_PORT_CONFLICT_FIX.md`](BUILD39_PORT_CONFLICT_FIX.md) - Detailed port fix

## Build History Context

- **Builds 32-36:** Various attempts to fix large file protocol corruption
- **Build 37:** Simplified to basic `fs_read_file()`, discovered timeout issue
- **Build 38:** Implemented internal debug buffer, discovered port conflict
- **Build 39:** Resolved port conflict, ready for timeout diagnosis

## Success Criteria

✅ **Build 39 Complete When:**
- [x] Port conflict resolved
- [x] Firmware builds successfully
- [x] Memory usage within limits
- [ ] Both servers accessible on hardware
- [ ] Debug log retrieval working
- [ ] Timeout root cause identified from debug output