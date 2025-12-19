# 9P Server Build History and Current Status

## Build Progress

### Build 4 - lwIP Polling Fix ✅
**Problem:** Incoming connections failed (No route to host)
**Root Cause:** Missing `cyw43_arch_poll()` calls
**Fix:** Added polling to Core 1, Core 0 menu, and Core 0 program loops
**Result:** Diagnostic server (port 1900) works, port 564 accepts connections
**Documentation:** [`doc/9P_LWIP_POLLING_FIX.md`](9P_LWIP_POLLING_FIX.md)

### Build 5 - Error Response Types ✅
**Problem:** Error responses used wrong message type
**Root Cause:** Handlers called `send_error()` without changing response type to Rerror
**Fix:** Added `resp->data[4] = Rerror;` before all 31 `send_error()` calls
**Result:** Error responses now use correct type (107)
**Documentation:** [`doc/9P_PROTOCOL_ERROR_FIX.md`](9P_PROTOCOL_ERROR_FIX.md)

### Build 6 - Error Message Headers ❌ (Incomplete)
**Problem:** Error messages missing type and tag fields
**Root Cause:** `p9_send_error()` only wrote size and error string
**Fix:** Added `p9_write_u8(&resp, Rerror)` and `p9_write_u16(&resp, tag)`
**Result:** Created duplicate headers (wrote header twice)
**Documentation:** [`doc/9P_MESSAGE_HEADER_BUG.md`](9P_MESSAGE_HEADER_BUG.md)

### Build 7 - Duplicate Header Fix ❌ (Incomplete)
**Problem:** Message headers written twice (once in main loop, once in error function)
**Root Cause:** `p9_process_message()` wrote header before checking message type validity
**Fix:** Check message type validity BEFORE writing response header
**Result:** Still had duplicate headers - incomplete fix

### Build 8 - Message API Fix ✅
**Problem:** `p9_send_error()` manually writing headers instead of using message API
**Root Cause:** Build 6 added manual header writes, Build 7 didn't remove them
**Fix:** Use `p9_msg_init_write()` and `p9_msg_finalize()` correctly
**Result:** Eliminated duplicate headers
**Documentation:** [`doc/9P_MESSAGE_API_BUG.md`](9P_MESSAGE_API_BUG.md)

### Build 9 - Double Header Read Fix ✅
**Problem:** Reading message header twice from buffer
**Root Cause:** `p9_msg_init_read()` reads header, then code read it again
**Fix:** Use `req.type` and `req.tag` from `p9_msg_init_read()`
**Result:** Correct header parsing
**Documentation:** [`doc/9P_DOUBLE_HEADER_READ_BUG.md`](9P_DOUBLE_HEADER_READ_BUG.md)

### Build 10 - Response Type Initialization ✅
**Problem:** Response initialized with type 0 instead of correct response type
**Root Cause:** `p9_msg_init_write()` called with 0 instead of `type + 1`
**Fix:** Initialize response with `resp_type = type + 1`
**Result:** Tversion/Rversion handshake works perfectly!
**Test Result:** ✅ Tversion → Rversion successful

### Build 11 - Missing Return Statement 🔧 (Testing)
**Problem:** Tattach handler not responding at all
**Root Cause:** Missing return statement at end of `p9_handle_attach()` success path
**Fix:** Added explicit return statement
**Result:** Awaiting hardware test
**Documentation:** [`doc/9P_MISSING_RETURN_BUG.md`](9P_MISSING_RETURN_BUG.md)

## Current Test Results (Build 6 or 7?)

```
Raw response (hex dump):
00000000: 2400 0000 6b00 3900 0000 006b 0039 1400  $...k.9....k.9..
00000010: 756e 6b6e 6f77 6e20 6d65 7373 6167 6520  unknown message 
00000020: 7479 7065                                type
```

**Analysis:**
- Size: `24 00 00 00` = 36 bytes
- **First header:** `6b 00 39` = Type 107 (Rerror), Tag 0x0039
- **Padding:** `00 00 00`
- **Second header:** `6b 00 39` = DUPLICATE!
- String length: `14 00` = 20 bytes
- Error: "unknown message type"

**This is Build 6 behavior** - duplicate headers still present!

## Why "unknown message type"?

The server is receiving Tversion (type 100 = 0x64) but responding with "unknown message type". This suggests:

1. **Message not reaching switch statement** - Type check failing before dispatch
2. **Type value corrupted** - Reading wrong value from buffer
3. **Enum mismatch** - `Tversion` constant not matching expected value

## Debugging Steps

### 1. Verify Build 7 Flashed

Check build number on PicoCalc display (lower right corner). Should show "Build 7".

If still showing Build 6:
```bash
# Force reflash
cp build/load81_picocalc.uf2 /media/RPI-RP2/
# Wait for device to reboot
# Verify build number on screen
```

### 2. Check Tversion Enum Value

In [`src/picocalc_9p_proto.h`](src/picocalc_9p_proto.h:37):
```c
typedef enum {
    Tversion = 100,  // Should be 100 (0x64)
    ...
} p9_msg_type_t;
```

Verify this matches what we're sending (0x64 in test script).

### 3. Add Debug Logging

If Build 7 is confirmed but still failing, we need to see what type value the server is reading.

Temporary debug code for [`src/picocalc_9p.c`](src/picocalc_9p.c:339):
```c
uint8_t type = p9_read_u8(&req);
uint16_t tag = p9_read_u16(&req);

// DEBUG: Log received values
printf("9P: Received type=%d (0x%02x), tag=%d\n", type, type, tag);
```

### 4. Verify Message Parsing

The test sends:
```
\x13\x00\x00\x00  - Size: 19
\x64              - Type: 100 (Tversion)
\xff\xff          - Tag: 65535 (NOTAG)
\x00\x20\x00\x00  - msize: 8192
\x08\x00          - version length: 8
39 50 32 30 30 30 2e 75  - "9P2000.u"
```

Server should read:
- size = 19 (0x13)
- type = 100 (0x64)
- tag = 65535 (0xffff)

## Most Likely Issues

### Issue 1: Build 7 Not Flashed
**Symptom:** Identical response to Build 6
**Solution:** Verify build number on screen, reflash if needed

### Issue 2: Type Value Mismatch
**Symptom:** Server reads different type value than sent
**Solution:** Add debug logging to see actual type value received

### Issue 3: Buffer Corruption
**Symptom:** Type value corrupted during read
**Solution:** Check `p9_read_u8()` implementation

## Next Steps

1. **Confirm Build 7 is running** - Check build number on PicoCalc screen
2. **If Build 6:** Reflash Build 7 and test again
3. **If Build 7:** Add debug logging to see what type value is being read
4. **Report findings:** Share build number and any debug output

## Expected Behavior (Build 7)

When Build 7 is properly flashed and working:

**Test script should show:**
```
✓ Response received!
Type: 101 (Rversion) ✓ Correct!
✓ Version negotiation successful!
```

**Mount should succeed:**
```bash
sudo mount -t 9p -o trans=tcp,port=564 192.168.178.122 /mnt/picocalc
# No error
ls /mnt/picocalc
# Shows SD card contents