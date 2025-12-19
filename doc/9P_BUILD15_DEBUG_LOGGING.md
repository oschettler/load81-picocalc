# Build 15: Debug Logging for Tattach Handler

**Date:** 2025-12-19  
**Build:** 15  
**Status:** Compiled Successfully

## Problem

Build 14 still showed no response to Tattach messages despite:
- Eliminating malloc() from the handler (using stack buffers)
- Fixing test script message size
- All previous bug fixes

The handler is either:
1. Crashing silently
2. Hitting an error path that doesn't send a response
3. Encountering an unexpected condition

## Solution

Added comprehensive printf debug logging throughout [`p9_handle_attach()`](../src/picocalc_9p_handlers.c:126-171) to trace execution:

```c
void p9_handle_attach(p9_client_t *client, p9_msg_t *req, p9_msg_t *resp) {
    printf("9P: Tattach handler started\n");
    
    /* Read request */
    uint32_t fid = p9_read_u32(req);
    uint32_t afid = p9_read_u32(req);
    printf("9P: Read fid=%u, afid=%u\n", fid, afid);
    
    /* Use stack buffers instead of malloc */
    char uname_buf[256];
    char aname_buf[256];
    
    printf("9P: Reading uname...\n");
    uint16_t uname_len = p9_read_string_buf(req, uname_buf, sizeof(uname_buf));
    printf("9P: uname_len=%u, uname='%s'\n", uname_len, uname_buf);
    
    printf("9P: Reading aname...\n");
    uint16_t aname_len = p9_read_string_buf(req, aname_buf, sizeof(aname_buf));
    printf("9P: aname_len=%u, aname='%s'\n", aname_len, aname_buf);
    
    if (uname_len == 0 || aname_len == 0 || req->error) {
        printf("9P: ERROR - invalid parameters (uname_len=%u, aname_len=%u, error=%d)\n", 
               uname_len, aname_len, req->error);
        resp->type = Rerror;
        send_error(resp, "invalid attach parameters");
        return;
    }
    
    printf("9P: Allocating FID %u...\n", fid);
    p9_fid_t *root_fid = p9_fid_alloc(&client->fid_table, fid);
    if (!root_fid) {
        printf("9P: ERROR - FID allocation failed\n");
        resp->type = Rerror;
        send_error(resp, "fid already in use");
        return;
    }
    
    printf("9P: Initializing root FID...\n");
    root_fid->type = P9_FID_TYPE_DIR;
    strcpy(root_fid->path, "/");
    root_fid->qid.type = P9_QTDIR;
    root_fid->qid.version = 0;
    root_fid->qid.path = 1;
    
    printf("9P: Writing QID response...\n");
    p9_write_qid(resp, &root_fid->qid);
    
    printf("9P: Setting client state to ATTACHED\n");
    client->state = P9_CLIENT_STATE_ATTACHED;
    
    printf("9P: Tattach handler completed successfully\n");
}
```

## Debug Points

The logging will reveal:
1. **Handler Entry**: Does the handler get called at all?
2. **Parameter Reading**: Are FID, AFID, uname, aname read correctly?
3. **Validation**: Does parameter validation pass?
4. **FID Allocation**: Does FID allocation succeed?
5. **QID Writing**: Does response writing work?
6. **Handler Exit**: Does the handler complete?

## Expected Outcomes

### If Handler Crashes
- Will see partial log output stopping at crash point
- Indicates memory corruption or stack overflow

### If Validation Fails
- Will see "ERROR - invalid parameters" message
- Indicates protocol parsing issue

### If FID Allocation Fails
- Will see "ERROR - FID allocation failed" message
- Indicates FID table issue

### If Handler Completes
- Will see "Tattach handler completed successfully"
- But no response sent → indicates issue in [`p9_process_message()`](../src/picocalc_9p.c:331-429)

## Testing Instructions

1. Flash Build 15:
   ```bash
   cp build/load81_picocalc.uf2 /media/RPI-RP2/
   ```

2. Wait for reboot and verify "Build 15" on screen

3. Connect USB serial to see debug output:
   ```bash
   screen /dev/ttyACM0 115200
   # or
   minicom -D /dev/ttyACM0 -b 115200
   ```

4. Run test in another terminal:
   ```bash
   ./test_9p_session.py
   ```

5. Observe debug output to identify failure point

## Files Modified

- [`src/picocalc_9p_handlers.c`](../src/picocalc_9p_handlers.c:126-171) - Added debug logging
- [`src/build_version.h`](../src/build_version.h) - Incremented to Build 15

## Next Steps

Based on debug output:
- **No output**: Handler not being called → check message dispatch
- **Partial output**: Identify crash location → fix memory issue
- **Complete output**: Response not being sent → check [`p9_process_message()`](../src/picocalc_9p.c:331-429)