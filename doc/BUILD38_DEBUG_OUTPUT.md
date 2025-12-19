# Build 38: Debug Output via Internal Log Buffer

## Overview
Build 38 adds comprehensive debug logging to diagnose the large file reading timeout issue. Debug messages are stored in an internal 8KB circular buffer and can be retrieved via the diagnostic server on port 1901.

## Debug Output Configuration

### Current Setup
- **DEBUG_OUTPUT**: Enabled in CMakeLists.txt
- **Output Method**: Internal circular buffer (8KB)
- **Access**: Via diagnostic server on port 1901
- **USB stdio**: Disabled (not needed)
- **Thread-safe**: Yes, uses mutex for Core 0/Core 1 safety

### Debug Logging Added
1. **File System Handler** ([`src/picocalc_fs_handler.c`](../src/picocalc_fs_handler.c)):
   - File open/close operations
   - File size detection
   - Memory allocation status
   - Chunked reading progress (offset, size, bytes read)
   - FAT32 error codes
   - EOF detection

2. **File Server** ([`src/picocalc_file_server.c`](../src/picocalc_file_server.c)):
   - CAT command execution flow
   - Path normalization
   - Data transmission progress
   - TCP write operations
   - Protocol markers (+DATA, +END)

## Accessing Debug Output

### Method 1: Using the Helper Script (Recommended)
```bash
# Retrieve debug log from PicoCalc
./tools/get_debug_log.sh 192.168.178.122

**Note:** The diagnostic server runs on port 1901 to avoid conflict with the load81r file server on port 1900.

# Or use default IP
./tools/get_debug_log.sh
```

### Method 2: Manual netcat
```bash
# Connect to diagnostic server
echo "LOG" | nc -w 2 192.168.178.122 1901
```

### Method 3: Interactive Session
```bash
# Connect interactively
nc 192.168.178.122 1901

# Type commands:
LOG      # Get debug log
STATS    # Get system statistics
HELP     # Show available commands
```

## Debug Log Buffer Details

- **Size**: 8KB circular buffer
- **Location**: RAM, accessible from both cores
- **Thread Safety**: Mutex-protected, non-blocking
- **Persistence**: Cleared on reboot
- **Overflow**: Old messages overwritten when buffer fills

## Troubleshooting

### Issue: Cannot Connect to Diagnostic Server

**Possible Causes:**
1. **WiFi not connected** - Check network connectivity
2. **Wrong IP address** - Verify PicoCalc IP with `ping`
3. **Firewall blocking** - Check firewall rules
4. **Server not running** - Diagnostic server starts automatically with WiFi

**Solutions:**
```bash
# Verify PicoCalc is reachable
ping 192.168.178.122

# Check if port 1901 is open
nc -zv 192.168.178.122 1901

# Try file server port to confirm WiFi is working
nc -zv 192.168.178.122 1900
```

### Issue: Debug Log is Empty

**Possible Causes:**
1. **No operations performed yet** - Debug log only fills when operations occur
2. **Buffer was cleared** - LOG command doesn't clear buffer, but reboot does

**Solution:**
Perform some operations first, then retrieve log:
```bash
# Trigger some file operations
tools/load81r/load81r.py 192.168.178.122 cat /load81/scorched.lua > /dev/null

# Then get debug log
./tools/get_debug_log.sh
```

## Debug Output Format

### Expected Output for CAT Command
```
[FILE_SERVER] Command: CAT /load81/scorched.lua
[FILE_SERVER] CAT command: args='/load81/scorched.lua'
[FILE_SERVER] CAT: Normalized path='/load81/scorched.lua'
[FILE_SERVER] CAT: Calling fs_read_file...
[FS] fs_read_file: Starting read of '/load81/scorched.lua'
[FS] fs_read_file: Opening file...
[FS] fs_read_file: File opened successfully
[FS] fs_read_file: File size = 12345 bytes
[FS] fs_read_file: Allocating 12345 bytes...
[FS] fs_read_file: Buffer allocated successfully
[FS] fs_read_file: Starting chunked read (chunk_size=4096)...
[FS] fs_read_file: Reading chunk at offset 0, size 4096
[FS] fs_read_file: Read 4096 bytes
[FS] fs_read_file: Reading chunk at offset 4096, size 4096
[FS] fs_read_file: Read 4096 bytes
[FS] fs_read_file: Reading chunk at offset 8192, size 4153
[FS] fs_read_file: Read 4153 bytes
[FS] fs_read_file: EOF reached
[FS] fs_read_file: Read complete, total_read=12345, file_size=12345
[FS] fs_read_file: File closed
[FS] fs_read_file: Success, returning 12345 bytes
[FILE_SERVER] CAT: File read successfully, size=12345 bytes
[FILE_SERVER] CAT: Calling send_data...
[FILE_SERVER] send_data: Starting, len=12345 bytes
[FILE_SERVER] send_data: Sending header: +DATA 12345
[FILE_SERVER] send_data: Sending chunk at offset 0, size 4096
[FILE_SERVER] send_data: Chunk sent, total sent=4096
[FILE_SERVER] send_data: Sending chunk at offset 4096, size 4096
[FILE_SERVER] send_data: Chunk sent, total sent=8192
[FILE_SERVER] send_data: Sending chunk at offset 8192, size 4153
[FILE_SERVER] send_data: Chunk sent, total sent=12345
[FILE_SERVER] send_data: All data sent, sending +END marker
[FILE_SERVER] send_data: Complete
[FILE_SERVER] CAT: send_data completed
[FILE_SERVER] CAT: Command complete
```

## Diagnostic Questions

If debug output is not appearing, check:

1. **Is USB cable connected to Pico2W micro USB?** (Not PicoCalc USB-C)
2. **Does `/dev/ttyACM0` (or similar) appear?** (`ls /dev/tty*`)
3. **Can you read from the device?** (`cat /dev/ttyACM0`)
4. **Is the baud rate correct?** (Should be 115200)
5. **Did WiFi initialize successfully?** (Check via network ping)

## Testing Procedure

### Step 1: Flash Firmware
```bash
# Copy UF2 to PicoCalc
cp build/load81_picocalc.uf2 /path/to/picocalc/mount/
```

### Step 2: Wait for WiFi Connection
The device will automatically connect to WiFi using credentials in `/load81/start.lua`.

### Step 3: Test File Operation
```bash
# Try to read a large file (this will timeout in Build 37)
tools/load81r/load81r.py 192.168.178.122 cat /load81/scorched.lua > /dev/null
```

### Step 4: Retrieve Debug Log
```bash
# Get the debug log to see what happened
./tools/get_debug_log.sh > debug_output.txt

# Review the log
cat debug_output.txt
```

### Step 5: Analyze Results
Look for these key indicators in the log:
- File open success/failure
- File size detection
- Memory allocation
- Read progress (which chunk failed?)
- TCP transmission status
- Error codes

## Expected Debug Output

For a successful 12KB file read, you should see:
```
[FS] fs_read_file: Starting read of '/load81/scorched.lua'
[FS] fs_read_file: Opening file...
[FS] fs_read_file: File opened successfully
[FS] fs_read_file: File size = 12345 bytes
[FS] fs_read_file: Allocating 12345 bytes...
[FS] fs_read_file: Buffer allocated successfully
[FS] fs_read_file: Starting chunked read (chunk_size=4096)...
[FS] fs_read_file: Reading chunk at offset 0, size 4096
[FS] fs_read_file: Read 4096 bytes
[FS] fs_read_file: Reading chunk at offset 4096, size 4096
[FS] fs_read_file: Read 4096 bytes
[FS] fs_read_file: Reading chunk at offset 8192, size 4153
[FS] fs_read_file: Read 4153 bytes
[FS] fs_read_file: EOF reached
[FS] fs_read_file: Read complete, total_read=12345, file_size=12345
[FS] fs_read_file: File closed
[FS] fs_read_file: Success, returning 12345 bytes
[FILE_SERVER] CAT: File read successfully, size=12345 bytes
[FILE_SERVER] send_data: Starting, len=12345 bytes
[FILE_SERVER] send_data: Sending header: +DATA 12345
[FILE_SERVER] send_data: Sending chunk at offset 0, size 4096
[FILE_SERVER] send_data: Chunk sent, total sent=4096
[FILE_SERVER] send_data: Sending chunk at offset 4096, size 4096
[FILE_SERVER] send_data: Chunk sent, total sent=8192
[FILE_SERVER] send_data: Sending chunk at offset 8192, size 4153
[FILE_SERVER] send_data: Chunk sent, total sent=12345
[FILE_SERVER] send_data: All data sent, sending +END marker
[FILE_SERVER] send_data: Complete
```

If the operation times out, the log will show exactly where it stopped.

## Next Steps

Once we have the debug log showing where the timeout occurs:
1. Identify the exact failure point
2. Measure timing if needed
3. Implement targeted fix
4. Verify with another test

## Build Information
- **Build**: 38
- **Date**: 2025-12-19
- **Binary Size**: 555KB
- **Text**: 571620 bytes
- **BSS**: 301868 bytes (58% of 520KB RAM)
- **Debug**: Enabled via internal log buffer
- **Debug Access**: Diagnostic server port 1901