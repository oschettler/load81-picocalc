# 9P Double Header Read Bug Fix (Build 9)

## Problem Discovery

Build 8 fixed the duplicate header write bug, but testing revealed the server was still responding with "unknown message type" to valid Tversion (type 100) messages.

Test output showed:
```
Raw response (hex dump):
1d 00 00 00  - Size: 29 bytes ✓
6b           - Type: 107 (Rerror) ✓  
00 39        - Tag: 0x3900 (wrong - should be 0xFFFF)
14 00        - String length: 20 ✓
"unknown message type"
```

The server was correctly sending error messages, but:
1. The tag was wrong (0x3900 instead of 0xFFFF)
2. The server thought type 100 (Tversion) was unknown

## Root Cause Analysis

The bug was in [`p9_process_message()`](../src/picocalc_9p.c:331-365). The function was **reading the message header twice**:

### First Read (Line 335)
```c
p9_msg_init_read(&req, client->rx_buffer, client->rx_len);
```

This function ([`p9_msg_init_read()`](../src/picocalc_9p_proto.c:18-37)) reads the header:
- Line 29: `msg->size = p9_read_u32(msg);`  - Reads size, advances pos to 4
- Line 30: `msg->type = p9_read_u8(msg);`   - Reads type, advances pos to 5
- Line 31: `msg->tag = p9_read_u16(msg);`   - Reads tag, advances pos to 7

After this, `req.pos = 7` (pointing at payload), `req.type = 100`, `req.tag = 0xFFFF`.

### Second Read (Lines 338-340) - THE BUG
```c
uint32_t size = p9_read_u32(&req);  // Reads bytes 7-10 (msize = 8192)
uint8_t type = p9_read_u8(&req);    // Reads byte 11 (version len = 8)
uint16_t tag = p9_read_u16(&req);   // Reads bytes 12-13 ("9P" = 0x5039)
```

This reads **payload data** as if it were the header!

### What Was Actually Read

For a Tversion message:
```
Offset  Data           Meaning
------  ----           -------
0-3     13 00 00 00    Size (19 bytes)
4       64             Type (100 = Tversion)
5-6     FF FF          Tag (NOTAG)
7-10    00 20 00 00    msize (8192) ← Read as "size"
11      08             version length (8) ← Read as "type"
12-13   39 50          "9P" ← Read as "tag" (0x5039)
14-20   "2000.u"       Rest of version string
```

So the code thought:
- `type = 8` (unknown type!)
- `tag = 0x5039` (garbage)

This is why the server responded with "unknown message type" and wrong tag.

## The Fix

Use the header values **already read** by `p9_msg_init_read()`:

```c
static void p9_process_message(p9_client_t *client) {
    p9_msg_t req, resp;
    
    /* Initialize request message from buffer - this reads the header */
    p9_msg_init_read(&req, client->rx_buffer, client->rx_len);
    
    /* Use header values already read by p9_msg_init_read */
    uint8_t type = req.type;
    uint16_t tag = req.tag;
    
    /* Now req.pos points at payload, ready for handlers to read */
    
    // ... rest of function
}
```

## Why This Wasn't Caught Earlier

1. The original code was written assuming manual header parsing
2. When `p9_msg_init_read()` was implemented to auto-parse headers, the manual parsing wasn't removed
3. The bug only manifested when testing actual protocol messages
4. Previous builds had other bugs that masked this issue

## Impact

This bug caused:
- **All incoming messages** to be misidentified
- Type values to be read from payload data instead of header
- Tag values to be corrupted
- The server to reject all valid 9P messages as "unknown"

## Verification

Build 9 should now correctly:
1. Read message type as 100 (Tversion)
2. Read tag as 0xFFFF (NOTAG)
3. Dispatch to `p9_handle_version()`
4. Respond with Rversion (type 101)

Expected test output:
```bash
./test_9p_protocol.sh
```

Should show:
```
Type: 101 (Rversion) ✓ Correct!
```

## Related Files

- [`src/picocalc_9p.c`](../src/picocalc_9p.c:331-340) - Fixed `p9_process_message()`
- [`src/picocalc_9p_proto.c`](../src/picocalc_9p_proto.c:18-37) - `p9_msg_init_read()` implementation
- [`doc/9P_MESSAGE_API_BUG.md`](9P_MESSAGE_API_BUG.md) - Previous message API bug

## Lessons Learned

1. **Don't duplicate work** - If an API function does something, don't do it again manually
2. **Understand API contracts** - `p9_msg_init_read()` reads the header AND advances position
3. **Check position tracking** - After `init_read`, `pos` points at payload, not header
4. **Test with real protocol data** - Unit tests with synthetic data might not catch this
5. **Read the implementation** - Understanding what `init_read` does internally is crucial

## Build History

- **Build 4**: Fixed network connectivity (lwIP polling)
- **Build 5**: Fixed error response types
- **Build 6**: Added headers to errors (created duplicates)
- **Build 7**: Fixed duplicate headers in `p9_process_message()`
- **Build 8**: Fixed message API usage in `p9_send_error()`
- **Build 9**: Fixed double header read in `p9_process_message()` ✓

This should be the final protocol parsing fix!