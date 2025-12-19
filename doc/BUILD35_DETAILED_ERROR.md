# Build 35: Detailed Error Reporting for Large File Debug

## Changes

Added detailed error reporting to the CAT command to help diagnose the large file reading issue without relying on USB debug output (which interferes with WiFi).

### Modified Files

**src/picocalc_file_server.c** (lines 304-312):
- Enhanced error message to include file size and bytes sent
- Format: `"<error_message> (size=<total_size>, sent=<bytes_sent>)"`
- This allows us to see exactly what's happening when the file read fails

## Testing Instructions

1. Flash Build 35:
```bash
cp build/load81_picocalc.uf2 /media/RPI-RP2/
```

2. Wait for WiFi connection (do NOT connect USB cable - it breaks WiFi)

3. Test the failing file:
```bash
tools/load81r/load81r.py <picocalc-ip> cat /load81/scorched.lua
```

4. **Look at the error message** - it will now show:
   - The error type (e.g., "I/O error", "Not found", etc.)
   - The total file size that was detected
   - How many bytes were successfully sent before the error

## Expected Output Examples

### If file opens but fails during read:
```
Error: I/O error (size=12500, sent=4096)
```
This would mean:
- File was opened successfully (size detected as 12500 bytes)
- First 4KB chunk was sent
- Error occurred on second chunk

### If file fails to open:
```
Error: File or directory not found (size=0, sent=0)
```
This would mean:
- File couldn't be opened at all
- No size was detected
- No data was sent

### If size detection fails:
```
Error: I/O error (size=0, sent=0)
```
This would mean:
- File opened but size couldn't be determined
- Or error occurred before first chunk

## Diagnostic Value

The detailed error will tell us:

1. **size=0, sent=0**: File open or size detection failed
2. **size>0, sent=0**: File opened, size detected, but first chunk failed
3. **size>0, sent>0**: File opened, some data sent, then failed mid-transfer
4. **size>0, sent=size**: All data sent but error flag set (shouldn't happen)

## Next Steps Based on Results

### If size=0:
- Problem is in file opening or size detection
- Check if file exists and is readable
- May be SD card issue

### If size>0, sent=0:
- File opens fine, but first read fails
- Problem in FAT32 read operation
- May be related to file position or cluster reading

### If size>0, sent>0 but sent<size:
- Partial read succeeded
- Problem occurs mid-file
- May be related to cluster boundaries or fragmentation

### If error is "File too large":
- File size exceeds FILE_SERVER_MAX_FILE_SIZE (currently 1MB)
- Need to increase limit or handle differently

## Known Issues

- USB debug output is DISABLED because it interferes with WiFi initialization
- When USB cable is connected to micro USB on Pico2W, WiFi fails with "Failed" message
- This is why we're using network-based error reporting instead

## Related Documents

- `doc/BUILD34_SINGLE_OPEN_FIX.md` - Previous attempt (eliminated double file open)
- `doc/BUILD32_LARGE_FILE_FIX.md` - Protocol corruption fix
- `doc/WIFI_DEBUG_BUILD30.md` - WiFi connection debugging