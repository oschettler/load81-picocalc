# Build 17: Connection Close Bug Fix

**Date:** 2025-12-19  
**Build:** 17  
**Status:** Compiled Successfully

## Problem

Build 16 revealed that the Tattach handler was never being called. Debug logs showed:
- Only "=== PicoCalc Boot ===" in the log
- "Connected Clients: 0" even after successful Tversion exchange
- No Tattach handler execution

This indicated the client connection was being closed immediately after Tversion succeeded.

## Root Cause

Found in [`p9_tcp_recv()`](../src/picocalc_9p.c:210-228) at lines 213-217:

```c
if (!p) {
    /* Connection closed by client */
    p9_client_close(client);
    return ERR_OK;
}
```

**The Bug**: We were incorrectly interpreting `p=NULL` as a connection close.

**The Reality**: In lwIP, `p9_tcp_recv()` is called with `p=NULL` and `err=ERR_OK` in two scenarios:
1. **Connection closed gracefully** by remote peer
2. **No data available** right now (receive buffer empty)

We were closing the connection in BOTH cases, when we should only close for case #1.

After sending Rversion, the client keeps the connection open (as required by 9P protocol) and waits to send Tattach. lwIP calls `p9_tcp_recv()` with `p=NULL` to indicate "no more data right now", but we incorrectly interpreted this as a connection close and terminated the client.

## Solution

Fixed the logic in [`p9_tcp_recv()`](../src/picocalc_9p.c:210-228):

```c
static err_t p9_tcp_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    p9_client_t *client = (p9_client_t *)arg;
    
    /* Check for errors first */
    if (err != ERR_OK) {
        if (p) {
            pbuf_free(p);
        }
        p9_client_close(client);
        return err;
    }
    
    /* p=NULL with err=ERR_OK means connection closed gracefully by remote */
    if (!p) {
        p9_client_close(client);
        return ERR_OK;
    }
    
    /* ... rest of function processes data ... */
}
```

**Key Changes:**
1. Check `err != ERR_OK` FIRST before checking `p`
2. Only close connection when `p=NULL` AND `err=ERR_OK` (graceful close)
3. If `err != ERR_OK`, close regardless of `p` value

## Expected Behavior After Fix

With Build 17:
1. Client connects → Tversion succeeds
2. Connection stays open (client count = 1)
3. Client sends Tattach → Handler executes
4. Debug log shows full Tattach execution trace
5. Rattach response sent successfully

## Testing Instructions

1. **Flash Build 17:**
   ```bash
   cp build/load81_picocalc.uf2 /media/RPI-RP2/
   ```

2. **Verify "Build 17" on screen**

3. **Run 9P test:**
   ```bash
   ./test_9p_session.py
   ```

4. **Check debug log:**
   ```bash
   curl http://192.168.178.122:1900
   ```

## Expected Results

**Test output should show:**
```
=== Test 1: Tversion ===
✓ Received Rversion

=== Test 2: Tattach ===
✓ Received Rattach
  QID type: 0x80 (directory)
```

**Debug log should show:**
```
=== PicoCalc Boot ===
9P: Tattach handler started
9P: Read fid=1, afid=4294967295
9P: Reading uname...
9P: uname_len=4, uname='user'
9P: Reading aname...
9P: aname_len=1, aname='/'
9P: Allocating FID 1...
9P: Initializing root FID...
9P: Writing QID response...
9P: Setting client state to ATTACHED
9P: Tattach handler completed successfully
```

**Diagnostic server should show:**
```
Connected Clients: 1
```

## Impact

This was a **critical bug** that prevented ANY 9P operations beyond Tversion. The connection was being closed immediately after version negotiation, making the entire 9P server non-functional.

## Files Modified

- [`src/picocalc_9p.c`](../src/picocalc_9p.c:210-228) - Fixed connection close logic
- [`src/build_version.h`](../src/build_version.h) - Incremented to Build 17

## Related Issues

This bug was discovered through the debug logging system implemented in Build 16:
- [`doc/9P_BUILD15_DEBUG_LOGGING.md`](9P_BUILD15_DEBUG_LOGGING.md) - Debug logging implementation
- [`src/picocalc_debug_log.c`](../src/picocalc_debug_log.c) - Debug log system
- [`src/picocalc_diag_server.c`](../src/picocalc_diag_server.c) - Network-accessible logs

## Next Steps

After confirming Build 17 works:
1. Verify Tattach succeeds
2. Test mount operation
3. Test file operations (read/write/list)
4. Performance testing
5. Create user documentation