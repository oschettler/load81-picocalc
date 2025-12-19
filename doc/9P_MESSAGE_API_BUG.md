# 9P Message API Bug Fix (Build 8)

## Problem Discovery

Build 7 was confirmed running on hardware but still exhibited duplicate headers in error responses. The test showed:

```
Hex dump: 24 00 00 00 6b 00 39 00 00 00 6b 00 39 14 00 "unknown message type"
                      ^^^^^^^^^^^       ^^^^^^^^^^
                      Header 1          Header 2 (duplicate)
```

## Root Cause Analysis

The bug was in [`p9_send_error()`](../src/picocalc_9p.c:437-452). The function was misusing the message API by manually writing header fields that the API already handles automatically.

### How the Message API Works

The 9P message API uses a two-phase approach:

1. **Initialization** - [`p9_msg_init_write()`](../src/picocalc_9p_proto.c:39-55):
   - Stores message type and tag in the `p9_msg_t` structure
   - Sets `pos = 7` to **reserve space** for the header
   - Does NOT write the header yet

2. **Finalization** - [`p9_msg_finalize()`](../src/picocalc_9p_proto.c:57-74):
   - Calculates final message size
   - Saves current position
   - Resets position to 0
   - **Writes the header** (size, type, tag) at the beginning
   - Restores position

### The Bug

In Build 7, `p9_send_error()` was doing this:

```c
static void p9_send_error(p9_client_t *client, uint16_t tag, const char *ename) {
    p9_msg_t resp;
    p9_msg_init_write(&resp, client->tx_buffer, P9_MAX_MSG_SIZE, Rerror, tag);
    
    p9_write_u32(&resp, 0);      // ❌ Manual write at pos=7
    p9_write_u8(&resp, Rerror);  // ❌ Manual write at pos=11
    p9_write_u16(&resp, tag);    // ❌ Manual write at pos=12
    p9_write_string(&resp, ename);
    
    p9_msg_finalize(&resp);      // ✓ Writes header at pos=0
    
    // Send...
}
```

This created:
- **Header 1** (bytes 0-6): Written by `p9_msg_finalize()`
- **Header 2** (bytes 7-13): Written manually
- **Error string** (bytes 14+): The actual payload

## The Fix

Remove the manual header writes since the API handles them:

```c
static void p9_send_error(p9_client_t *client, uint16_t tag, const char *ename) {
    p9_msg_t resp;
    p9_msg_init_write(&resp, client->tx_buffer, P9_MAX_MSG_SIZE, Rerror, tag);
    
    /* p9_msg_init_write already reserved space for header (size, type, tag)
     * Just write the error string - p9_msg_finalize will fill in the header */
    p9_write_string(&resp, ename);
    
    p9_msg_finalize(&resp);
    
    // Send...
}
```

Now the message structure is correct:
- **Header** (bytes 0-6): size[4] type[1] tag[2]
- **Error string** (bytes 7+): length[2] + string data

## Why This Wasn't Caught Earlier

1. **Build 6** introduced the manual header writes to fix missing type/tag fields
2. **Build 7** fixed duplicate headers in `p9_process_message()` but didn't address `p9_send_error()`
3. The confusion arose from not understanding that `p9_msg_init_write()` **reserves** space rather than writing immediately

## Verification

Build 8 should now produce correct error messages:

```
Expected hex dump for "unknown message type":
1F 00 00 00  - Size (31 bytes total)
6B           - Type (107 = Rerror)
FF FF        - Tag (0xFFFF = NOTAG)
14 00        - String length (20)
"unknown message type"
```

## Related Files

- [`src/picocalc_9p.c`](../src/picocalc_9p.c:437-449) - Fixed `p9_send_error()`
- [`src/picocalc_9p_proto.c`](../src/picocalc_9p_proto.c:39-74) - Message API implementation
- [`doc/9P_MESSAGE_HEADER_BUG.md`](9P_MESSAGE_HEADER_BUG.md) - Previous header bug in Build 6

## Testing

Flash Build 8 and run:
```bash
./test_9p_protocol.sh
```

Expected output:
```
Type: 101 (Rversion) ✓ Correct!
```

## Lessons Learned

1. **Read the API documentation carefully** - The message API's two-phase design wasn't immediately obvious
2. **Understand initialization vs. finalization** - `init_write` reserves space, `finalize` fills it
3. **Don't mix manual and API writes** - Let the API handle what it's designed to handle
4. **Test incrementally** - Each fix should be verified before moving to the next issue