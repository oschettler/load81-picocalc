# 9P Protocol Error Response Fix

## Problem Identified

**Build 4 Issue:** Mount attempts failed with "Keine Route zum Zielrechner" (No route to host), and the device stopped responding after ~1 minute.

## Root Cause

The 9P protocol handlers were sending error responses with **incorrect message types**. When an error occurred, the code was:

1. Setting response type to `request_type + 1` (line 349 in `picocalc_9p.c`)
2. Then calling `send_error()` which writes an error string
3. But **NOT changing the type to `Rerror` (107)**

### Example of the Bug

When a client sent `Tauth` (102):
- Response type was set to 103 (`Rauth`)
- But we sent an error string instead of auth data
- Client expected `Rerror` (107) for errors

This violated the 9P protocol specification, causing clients to:
- Receive malformed responses
- Timeout or disconnect
- Potentially crash the server

## The Fix (Build 5)

Added `resp->data[4] = Rerror;` before **every** `send_error()` call in all 13 message handlers:

### Files Modified
- `src/picocalc_9p_handlers.c` - Fixed all error paths in:
  - `p9_handle_version()` - 1 error path
  - `p9_handle_auth()` - 1 error path (already fixed in Build 4)
  - `p9_handle_attach()` - 2 error paths
  - `p9_handle_walk()` - 5 error paths
  - `p9_handle_open()` - 2 error paths
  - `p9_handle_create()` - 4 error paths
  - `p9_handle_read()` - 4 error paths
  - `p9_handle_write()` - 5 error paths
  - `p9_handle_remove()` - 2 error paths
  - `p9_handle_stat()` - 2 error paths
  - `p9_handle_wstat()` - 3 error paths

**Total: 31 error paths fixed**

## Protocol Specification

According to 9P2000.u specification:
- All error responses MUST use message type `Rerror` (107)
- Error responses contain: `size[4] type[1] tag[2] ename[s]`
- The `type` field MUST be 107, regardless of the request type

## Expected Behavior After Fix

1. **Version Negotiation:**
   - Client sends `Tversion` (100)
   - Server responds `Rversion` (101) with negotiated parameters

2. **Authentication (if attempted):**
   - Client sends `Tauth` (102)
   - Server responds `Rerror` (107) with "authentication not required"
   - Client proceeds without auth

3. **Attach:**
   - Client sends `Tattach` (104)
   - Server responds `Rattach` (105) with root QID

4. **Any Error:**
   - Server ALWAYS responds with `Rerror` (107)
   - Never sends error string with wrong message type

## Testing Plan

1. **Verify mount succeeds:**
   ```bash
   sudo mount -t 9p -o trans=tcp,port=564 192.168.178.122 /mnt/picocalc
   ```

2. **Check dmesg for protocol errors:**
   ```bash
   dmesg | tail -20
   ```

3. **Test basic operations:**
   ```bash
   ls -la /mnt/picocalc
   touch /mnt/picocalc/test.txt
   echo "Hello" > /mnt/picocalc/test.txt
   cat /mnt/picocalc/test.txt
   ```

## Related Issues

- **Build 4:** Fixed lwIP polling (diagnostic server worked)
- **Build 5:** Fixed protocol error responses (should fix mount)

## References

- 9P2000.u Specification: http://ericvh.github.io/9p-rfc/rfc9p2000.u.html
- Message Types: Lines 36-65 in `src/picocalc_9p_proto.h`
- Error Handling: Line 36 in `src/picocalc_9p_handlers.c`