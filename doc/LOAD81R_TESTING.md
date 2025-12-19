# LOAD81R Testing Guide

This document provides comprehensive testing procedures for the LOAD81R remote shell system.

## Build Information

- **Build Number**: 23
- **Firmware**: `build/load81_picocalc.uf2`
- **Server Port**: 1900
- **Protocol**: Text-based over TCP/IP

## Prerequisites

### Hardware
- PicoCalc device with RP2350
- SD card inserted and formatted (FAT32)
- WiFi configured in `wifi_config.txt`
- USB cable for flashing

### Software
- Python 3.6 or later
- Network connectivity to PicoCalc
- Terminal emulator (optional, for debugging)

## Installation

### 1. Flash Firmware

```bash
# Put PicoCalc in bootloader mode (hold BOOTSEL, press RESET)
# Mount will appear as RPI-RP2

# Flash firmware
cp build/load81_picocalc.uf2 /media/RPI-RP2/

# Device will reboot automatically
```

### 2. Configure WiFi

Edit the startup script on the SD card:

```bash
# Mount SD card and edit /load81/start.lua
nano /media/SDCARD/load81/start.lua

# Update WiFi credentials:
local WIFI_SSID = "YourNetworkName"
local WIFI_PASSWORD = "YourPassword"
```

### 3. Verify WiFi Connection

The device should:
1. Boot and execute `/load81/start.lua`
2. Connect to WiFi via Lua `wifi.connect()`
3. File server starts automatically after WiFi connection
4. Server listens on port 1900

Check serial output for:
```
=== LOAD81 Startup Script ===
Connecting to WiFi: YourNetworkName
WiFi connected!
IP Address: 192.168.x.x
File server started on port 1900
```

### 3. Find Device IP Address

Method 1 - Serial console:
```bash
# Connect to serial port
screen /dev/ttyACM0 115200
# Look for "IP: x.x.x.x" message
```

Method 2 - Network scan:
```bash
# Scan local network
nmap -p 1900 192.168.1.0/24
```

Method 3 - Router admin panel:
- Look for device named "picocalc" or similar

## Basic Testing

### Test 1: Connection

```bash
# Test TCP connection
nc -v 192.168.1.100 1900

# Should see connection established
# Type: HELLO
# Should receive: +OK LOAD81R v1.0
```

### Test 2: Python Client

```bash
cd tools/load81r

# Test connection
python3 -c "
from client import Load81Client
client = Load81Client('192.168.1.100')
if client.connect():
    print('Connected!')
    print('PWD:', client.pwd())
    client.disconnect()
else:
    print('Connection failed')
"
```

### Test 3: Interactive Shell

```bash
./load81r.py 192.168.1.100

# Should see:
# Connecting to 192.168.1.100:1900...
# Connected to PicoCalc
# Type 'help' for available commands
# 
# load81r:/>
```

## Command Testing

### File Operations

```bash
# Start shell
./load81r.py 192.168.1.100

# Test ls
load81r:/> ls
# Should list root directory contents

# Test cd
load81r:/> cd load81
load81r:/load81> ls
# Should list load81 directory

# Test cat
load81r:/load81> cat nex.lua
# Should display file contents

# Test mkdir
load81r:/> mkdir test
load81r:/> cd test
load81r:/test> ls
# Should show empty directory
```

### File Transfer

```bash
# Create test file locally
echo "Hello from host" > test.txt

# Upload file
./load81r.py 192.168.1.100 cp ./test.txt remote:/test/hello.txt

# Verify upload
./load81r.py 192.168.1.100 cat /test/hello.txt
# Should display: Hello from host

# Download file
./load81r.py 192.168.1.100 cp remote:/test/hello.txt ./downloaded.txt

# Verify download
cat downloaded.txt
# Should display: Hello from host
```

### Directory Sync

```bash
# Create test directory
mkdir -p testdir/subdir
echo "File 1" > testdir/file1.txt
echo "File 2" > testdir/subdir/file2.txt

# Upload directory
./load81r.py 192.168.1.100 rsync ./testdir /test/uploaded

# Verify upload
./load81r.py 192.168.1.100 ls /test/uploaded
./load81r.py 192.168.1.100 cat /test/uploaded/file1.txt

# Download directory
./load81r.py 192.168.1.100 rsync /test/uploaded ./downloaded

# Verify download
ls -R downloaded/
cat downloaded/file1.txt
```

### Lua REPL

```bash
./load81r.py 192.168.1.100 repl

lua> print("Hello from Lua!")
Hello from Lua!

lua> x = 42
lua> print(x * 2)
84

lua> for i=1,5 do print(i) end
1
2
3
4
5

lua> .exit
```

### File Editing

```bash
# Set editor
export EDITOR=nano

# Edit remote file
./load81r.py 192.168.1.100 edit /test/myfile.lua

# Make changes in editor, save and exit
# File should be uploaded automatically
```

## Stress Testing

### Multiple File Operations

```bash
# Create 100 small files
for i in {1..100}; do
    echo "File $i" | ./load81r.py 192.168.1.100 cp /dev/stdin remote:/test/file$i.txt
done

# List all files
./load81r.py 192.168.1.100 ls /test

# Download all files
for i in {1..100}; do
    ./load81r.py 192.168.1.100 cp remote:/test/file$i.txt ./file$i.txt
done
```

### Large File Transfer

```bash
# Create 1MB test file
dd if=/dev/urandom of=large.bin bs=1M count=1

# Upload
time ./load81r.py 192.168.1.100 cp ./large.bin remote:/test/large.bin

# Download
time ./load81r.py 192.168.1.100 cp remote:/test/large.bin ./large_downloaded.bin

# Verify
md5sum large.bin large_downloaded.bin
```

### Concurrent Operations

```bash
# Test single-client enforcement
# Terminal 1:
./load81r.py 192.168.1.100
# Keep shell open

# Terminal 2:
./load81r.py 192.168.1.100
# Should fail with "Server busy" or similar
```

## Error Testing

### Invalid Commands

```bash
./load81r.py 192.168.1.100

load81r:/> invalid_command
# Should show error message

load81r:/> cat /nonexistent/file.txt
# Should show "file not found" error

load81r:/> cd /nonexistent/directory
# Should show "directory not found" error
```

### Path Traversal

```bash
# Test security - should be blocked
./load81r.py 192.168.1.100 cat /../../../etc/passwd
# Should fail with path validation error

./load81r.py 192.168.1.100 cat /test/../../sensitive
# Should fail with path validation error
```

### Network Issues

```bash
# Test connection timeout
./load81r.py 192.168.1.999 ls
# Should timeout after 5 seconds

# Test disconnection handling
# Start shell, then power off PicoCalc
./load81r.py 192.168.1.100
load81r:/> ls
# Disconnect device
load81r:/> ls
# Should show connection error
```

## Performance Benchmarks

### File Transfer Speed

```bash
# 1MB file
dd if=/dev/zero of=1mb.bin bs=1M count=1
time ./load81r.py 192.168.1.100 cp ./1mb.bin remote:/test/1mb.bin
# Record time

# 10MB file
dd if=/dev/zero of=10mb.bin bs=1M count=10
time ./load81r.py 192.168.1.100 cp ./10mb.bin remote:/test/10mb.bin
# Record time
```

### Directory Sync Speed

```bash
# Create test directory with many files
mkdir -p perftest
for i in {1..100}; do
    dd if=/dev/urandom of=perftest/file$i.bin bs=1K count=10
done

# Time sync
time ./load81r.py 192.168.1.100 rsync ./perftest /test/perftest
```

### REPL Response Time

```bash
# Test REPL latency
./load81r.py 192.168.1.100 repl

lua> -- Time simple operations
lua> print(os.clock())
lua> for i=1,1000 do x=i*2 end
lua> print(os.clock())
# Calculate difference
```

## Troubleshooting

### Server Not Starting

**Symptoms**: Cannot connect to port 1900

**Checks**:
1. Verify WiFi connection: Check serial output for IP address
2. Check server initialization: Look for "File server started" message
3. Verify port: Try `nc -v <ip> 1900`
4. Check firewall: Ensure port 1900 is not blocked

**Solutions**:
- Reflash firmware
- Check WiFi configuration in `wifi_config.txt`
- Verify SD card is inserted and readable

### Connection Refused

**Symptoms**: "Connection refused" error

**Checks**:
1. Ping device: `ping <ip>`
2. Check port: `nmap -p 1900 <ip>`
3. Verify server is running: Check serial output

**Solutions**:
- Restart device
- Check if another client is connected (single-client mode)
- Verify firmware build includes file server

### File Operations Fail

**Symptoms**: "Cannot read file" or "Cannot write file" errors

**Checks**:
1. Verify SD card is inserted
2. Check file system: Try listing root directory
3. Verify paths are correct (case-sensitive)
4. Check available space

**Solutions**:
- Reinsert SD card
- Format SD card (FAT32)
- Check file permissions
- Free up space on SD card

### REPL Not Working

**Symptoms**: REPL commands return errors or no response

**Checks**:
1. Verify Lua interpreter is running
2. Check inter-core communication
3. Try simple expressions: `print(1+1)`

**Solutions**:
- Restart device
- Check Core 0 is running Lua interpreter
- Verify FIFO communication is working

### Slow Performance

**Symptoms**: File transfers are very slow

**Checks**:
1. Check WiFi signal strength
2. Verify network bandwidth
3. Test with smaller files
4. Check SD card speed

**Solutions**:
- Move closer to WiFi router
- Use 5GHz WiFi if available
- Use faster SD card (Class 10 or UHS)
- Reduce file sizes

## Success Criteria

The LOAD81R system is working correctly if:

- ✓ Server starts automatically after WiFi connection
- ✓ Client can connect and authenticate
- ✓ All file operations work (ls, cat, mkdir, rm)
- ✓ File upload/download works correctly
- ✓ Directory sync works recursively
- ✓ Lua REPL executes code and returns results
- ✓ File editing works with local editor
- ✓ Single-client enforcement works
- ✓ Path validation prevents directory traversal
- ✓ Error handling is robust
- ✓ Performance is acceptable (>100KB/s transfer)

## Known Limitations

1. **Single Client**: Only one client can connect at a time
2. **No Authentication**: No password protection (use trusted network)
3. **No Encryption**: Data transmitted in plain text
4. **Memory Constraints**: Large files may cause issues
5. **SD Card Speed**: Limited by SD card and SPI interface
6. **WiFi Range**: Limited by CYW43 module capabilities

## Next Steps

After successful testing:

1. Document any issues found
2. Create example programs and tutorials
3. Add advanced features (if needed):
   - Multi-client support
   - Authentication
   - Compression
   - Progress indicators
4. Optimize performance
5. Create user documentation

## Support

For issues or questions:
- Check serial console output for errors
- Review architecture document: `plans/load81r_architecture.md`
- Review implementation plan: `plans/load81r_implementation_plan.md`
- Check source code comments in `src/picocalc_file_server.c`