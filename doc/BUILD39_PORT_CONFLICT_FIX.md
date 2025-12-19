# Build 39: Port Conflict Resolution

## Problem Identified

**Root Cause:** Both the load81r file server and the diagnostic server were configured to use **port 1900**, causing a port conflict that prevented the diagnostic server from starting.

### Impact
- The diagnostic server could not bind to port 1900 (already in use by file server)
- Debug log output was inaccessible
- Unable to diagnose the large file reading timeout issue

### Discovery
While investigating why debug output was empty in Build 38, we discovered:
- [`src/picocalc_file_server.h:19`](../src/picocalc_file_server.h:19) - `FILE_SERVER_PORT 1900`
- [`src/picocalc_diag_server.c:22`](../src/picocalc_diag_server.c:22) - `DIAG_PORT 1900` (original)

The file server header even had a comment "Replaces the diagnostic server on port 1900" - but we need BOTH servers running simultaneously to access debug logs.

## Solution

Changed diagnostic server to use **port 1901** to avoid conflict with file server on port 1900.

### Files Modified

1. **[`src/picocalc_diag_server.c`](../src/picocalc_diag_server.c:22)**
   - Changed `DIAG_PORT` from 1900 to 1901

2. **[`tools/get_debug_log.sh`](../tools/get_debug_log.sh:6)**
   - Updated PORT from 1900 to 1901

3. **[`doc/BUILD38_DEBUG_OUTPUT.md`](../doc/BUILD38_DEBUG_OUTPUT.md)**
   - Updated documentation to reflect port 1901
   - Added note about port conflict avoidance

## Port Allocation

| Service | Port | Purpose |
|---------|------|---------|
| load81r File Server | 1900 | Remote shell and file operations |
| Diagnostic Server | 1901 | Debug log access and system diagnostics |

## Testing Instructions

After rebuilding and flashing firmware:

1. **Verify file server is accessible:**
   ```bash
   echo "HELLO" | nc 192.168.178.122 1900
   ```
   Expected: `+OK load81r/1.0`

2. **Verify diagnostic server is accessible:**
   ```bash
   echo "status" | nc 192.168.178.122 1901
   ```
   Expected: System status and debug log output

3. **Use helper script:**
   ```bash
   ./tools/get_debug_log.sh 192.168.178.122
   ```

## Next Steps

With both servers now accessible:
1. Rebuild firmware with port conflict resolved
2. Flash to PicoCalc
3. Retrieve debug log to diagnose large file timeout issue
4. Analyze where the file reading operation is hanging

## Related Issues

- Build 37-38: Large file reading timeout (≥8KB files)
- Build 38: Debug output implementation with internal buffer
- This fix enables debugging of the timeout issue