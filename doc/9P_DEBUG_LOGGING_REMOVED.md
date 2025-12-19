# Build 20: Debug Logging Removed from Interrupt Context

## Problem

Build 19 testing showed that while the system no longer crashed, Tattach still received no response and the diagnostic server timed out afterward. The root cause was that **all debug logging was being silently dropped** due to the non-blocking mutex implementation, leaving us with zero visibility into what was happening.

**Analysis revealed:**
- Build 19 made `debug_log()` use `mutex_try_enter()` instead of `mutex_enter_blocking()`
- This prevented crashes but caused all log entries to be silently dropped when the mutex was busy
- The Tattach handler had **10 debug_log() calls** (lines 127-179) that were all being skipped
- Without logging, we couldn't diagnose why the handler was failing or if it was even executing

## Root Cause

The fundamental issue is that **the entire message processing pipeline runs in interrupt context**:

1. [`p9_tcp_recv()`](../src/picocalc_9p.c:210) - lwIP callback (interrupt context)
2. [`p9_process_message()`](../src/picocalc_9p.c:334) - called from interrupt context
3. [`p9_handle_attach()`](../src/picocalc_9p_handlers.c:126) - called from interrupt context
4. All handler operations including `tcp_write()` and `tcp_output()` - interrupt context

**The problem with debug logging in interrupt context:**
- `debug_log()` uses a mutex to protect the circular buffer
- In Build 18, it used `mutex_enter_blocking()` → hard fault crash
- In Build 19, it used `mutex_try_enter()` → silently drops all logs
- Either way, debug logging doesn't work reliably in interrupt context

**Additional concerns:**
- `tcp_write()` and `tcp_output()` at lines 422-424 might also be problematic in interrupt context
- Memory allocation, file I/O, and other operations in handlers are all running in interrupt context
- This violates embedded systems best practices for interrupt handlers

## The Fix

**Immediate fix for Build 20:** Remove all `debug_log()` calls from the Tattach handler to eliminate any potential issues caused by the logging system.

### Changes Made

Removed 10 debug_log() calls from [`p9_handle_attach()`](../src/picocalc_9p_handlers.c:126):

```c
/* Before (Build 19): */
void p9_handle_attach(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp) {
    debug_log("9P: Tattach handler started");
    // ... 8 more debug_log() calls ...
    debug_log("9P: Tattach handler completed successfully");
}

/* After (Build 20): */
void p9_handle_attach(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp) {
    /* Read request */
    uint32_t fid = p9_read_u32(req);
    // ... handler logic without any debug_log() calls ...
    client->state = P9_CLIENT_STATE_ATTACHED;
}
```

## Expected Results with Build 20

1. ✅ **No crashes**: System remains stable (already working in Build 19)
2. ✅ **No mutex contention**: No debug logging means no mutex operations
3. ✅ **Handler executes cleanly**: No potential side effects from logging
4. ✅ **Tattach should succeed**: If the handler logic is correct, response should be sent
5. ✅ **Diagnostic server stays responsive**: No system-wide issues

## Long-Term Solution Needed

The proper fix requires **architectural changes** to move message processing out of interrupt context:

### Option 1: Deferred Processing (Recommended)
```c
static err_t p9_tcp_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    /* In interrupt context - just queue the data */
    // Copy data to queue
    // Signal worker thread
    return ERR_OK;
}

/* Separate worker thread on Core 1 */
void p9_worker_thread(void) {
    while (1) {
        // Wait for queued messages
        // Process messages in non-interrupt context
        // Send responses
    }
}
```

### Option 2: lwIP TCPIP Thread Mode
- Use lwIP's built-in threading support
- Messages processed in lwIP's tcpip_thread
- Requires more memory but cleaner architecture

### Option 3: Minimal Interrupt Handler
- Only copy data in interrupt context
- Set flag for main loop to process
- Process in Core 1 main loop (non-interrupt)

## Testing Instructions for Build 20

```bash
# Flash Build 20
cp build/load81_picocalc.uf2 /media/RPI-RP2/

# Wait for reboot, verify "Build 20" on screen

# Test 9P protocol
./test_9p_session.py
# Expected:
# ✓ Received Rversion
# ✓ Received Rattach  ← Should work now!

# If Tattach succeeds, try mounting
sudo mount -t 9p -o trans=tcp,port=564 192.168.178.122 /mnt/picocalc
ls /mnt/picocalc
```

## Why This Should Work

**Hypothesis:** The debug logging system was causing subtle issues even with non-blocking mutexes:
- Mutex operations have overhead even when they succeed
- Failed mutex attempts might affect timing
- The logging system might be interfering with lwIP's internal state

**By removing all debug logging from the handler:**
- Eliminates all mutex operations
- Reduces handler execution time
- Removes any potential side effects
- Allows us to test if the core handler logic is correct

## Next Steps

1. **Test Build 20** - Verify Tattach succeeds without debug logging
2. **If successful** - Proceed to test full filesystem operations
3. **If still failing** - Need to investigate other potential issues:
   - `tcp_write()` / `tcp_output()` in interrupt context
   - FID allocation or filesystem operations
   - Response message construction
4. **Long-term** - Implement proper deferred message processing architecture

## Files Modified

- [`src/picocalc_9p_handlers.c`](../src/picocalc_9p_handlers.c): Removed all debug_log() calls from p9_handle_attach()

## Build Information

- **Build Number**: 20
- **Date**: 2024-12-19
- **Status**: Compiled successfully, ready for testing
- **Critical Change**: Removed debug logging from interrupt context handlers