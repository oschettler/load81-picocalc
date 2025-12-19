# 9P Response Type Corruption Bug (Build 12)

## Problem

Tattach handler still not responding after Build 11 fix. Test showed:
```
=== Test 2: Tattach ===
Sending Tattach: [24 bytes]
✗ No response received
```

## Root Cause Analysis

### The Bug

All error handlers were using `resp->data[4] = Rerror` to set error response type, but this directly corrupts the message buffer instead of updating the message structure's type field.

**What was happening:**

1. `p9_process_message()` initializes response with `p9_msg_init_write(&resp, ..., Rattach, tag)`
   - Sets `resp.type = 105` (Rattach)
   - Reserves header space at positions 0-6
   - Sets `resp.pos = 7` (ready for payload)

2. Handler hits error, executes: `resp->data[4] = Rerror` (107)
   - **Corrupts byte 4 of the buffer** (which is reserved for header)
   - **Does NOT update `resp.type`** (still 105!)

3. Handler calls `send_error(resp, "error message")`
   - Writes error string starting at position 7

4. Handler returns to `p9_process_message()`

5. `p9_msg_finalize(&resp)` writes header:
   - Writes `resp.type` (still 105!) at position 4
   - **Overwrites the corrupted byte with wrong type**

6. Response sent with type 105 (Rattach) but contains error message
   - Client expects Rattach with QID
   - Gets Rattach with error string
   - **Protocol violation - client confused/crashes**

### Why This Caused "No Response"

When the handler hit an error (likely malloc failure for string allocation), it:
1. Set `resp->data[4] = Rerror` (corrupted buffer)
2. Wrote error string
3. Returned
4. `p9_msg_finalize()` overwrote position 4 with `resp.type` (105)
5. Sent malformed Rattach message
6. Client received invalid response and dropped connection

## The Fix

Changed ALL 29 error handlers from:
```c
resp->data[4] = Rerror;
send_error(resp, "error message");
```

To:
```c
resp->type = Rerror;
send_error(resp, "error message");
```

This ensures:
1. `resp.type` is updated correctly
2. `p9_msg_finalize()` writes correct type to buffer
3. Response has proper Rerror type (107)
4. Client receives valid error response

## Files Modified

- [`src/picocalc_9p_handlers.c`](../src/picocalc_9p_handlers.c) - Fixed 29 occurrences across all handlers:
  - `p9_handle_version()` - 1 error path
  - `p9_handle_auth()` - 1 error path (intentional)
  - `p9_handle_attach()` - 2 error paths
  - `p9_handle_walk()` - 5 error paths
  - `p9_handle_open()` - 2 error paths
  - `p9_handle_create()` - 4 error paths
  - `p9_handle_read()` - 4 error paths
  - `p9_handle_write()` - 4 error paths
  - `p9_handle_remove()` - 2 error paths
  - `p9_handle_stat()` - 2 error paths
  - `p9_handle_wstat()` - 2 error paths

## Why This Bug Existed

This bug was introduced in Build 5 when we added error type setting. The original code used:
```c
resp->data[4] = Rerror;
```

This worked in Build 5 because `p9_msg_finalize()` wasn't overwriting it. But after Build 8's message API refactoring, `p9_msg_finalize()` started writing the header from `resp.type`, which overwrote the manual buffer modification.

## Expected Result

After Build 12:
- Error responses will have correct Rerror type (107)
- Tattach should either succeed or return proper Rerror
- No more protocol violations from type mismatches

## Testing

Run the Python test script:
```bash
./test_9p_session.py
```

Expected outcomes:
1. **If Tattach succeeds:**
   ```
   === Test 2: Tattach ===
   Received Rattach:
     Type: 105 (expected 105) ✓
   ```

2. **If Tattach fails (e.g., malloc error):**
   ```
   === Test 2: Tattach ===
   Received Rerror:
     Type: 107 (Rerror)
     Error: [specific error message]
   ```

Either way, we'll get a valid response instead of silence!

## Build Information

- **Build Number**: 12
- **Files Modified**: `src/picocalc_9p_handlers.c`
- **Lines Changed**: 29 (all error response type assignments)
- **Compilation**: Successful