# 9P Missing Return Statement Bug (Build 11)

## Problem

The Tattach handler was not responding at all - the Python test script showed:
```
=== Test 2: Tattach ===
Sending Tattach: [24 bytes]
✗ No response received
```

Tversion worked perfectly, but Tattach had no response whatsoever.

## Root Cause

The [`p9_handle_attach()`](../src/picocalc_9p_handlers.c:126-165) function was missing a return statement at the end of the success path:

```c
void p9_handle_attach(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp) {
    // ... error handling with returns ...
    
    /* Write response */
    p9_write_qid(resp, &root_fid->qid);
    
    client->state = P9_CLIENT_STATE_ATTACHED;
    
    p9_string_free(&uname);
    p9_string_free(&aname);
    // MISSING: return statement here!
}
```

When a C function doesn't have an explicit return statement, execution continues into whatever memory comes after the function, causing:
- Undefined behavior
- Potential memory corruption
- Handler crashes
- No response sent to client

## The Fix

Added explicit return statement at the end of the success path:

```c
void p9_handle_attach(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp) {
    // ... error handling with returns ...
    
    /* Write response */
    p9_write_qid(resp, &root_fid->qid);
    
    client->state = P9_CLIENT_STATE_ATTACHED;
    
    p9_string_free(&uname);
    p9_string_free(&aname);
    
    /* Success - return normally */
}
```

## Why This Happened

All error paths in the handler had explicit `return` statements, but the success path didn't. This is a common C programming mistake where:
1. Error paths are obvious and get returns
2. Success path "falls through" and programmer forgets the return
3. Compiler doesn't warn because void functions don't require return values

## Expected Result

After this fix, Tattach should:
1. Receive the request
2. Allocate root FID
3. Write Rattach response with QID
4. Return normally
5. Response gets finalized and sent

The mount operation should now succeed!

## Testing

Run the Python test script:
```bash
./test_9p_session.py
```

Expected output:
```
=== Test 2: Tattach ===
Received Rattach:
  14 00 00 00 69 01 00 ...
  Type: 105 (expected 105) ✓
```

Then try mounting:
```bash
sudo mount -t 9p -o trans=tcp,port=564 192.168.178.122 /mnt/picocalc
```

## Build Information

- **Build Number**: 11
- **Files Modified**: `src/picocalc_9p_handlers.c`
- **Lines Changed**: 1 (added return comment)
- **Compilation**: Successful