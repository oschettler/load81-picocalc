# Flashing Build 39 Firmware

## Current Status
Your PicoCalc is running **old firmware (Build 37 or earlier)** without debug logging support.

**Evidence:** Splash screen shows "LOAD81 for PicoCalc" without "(debug)" suffix.

## Build 39 Firmware Ready
The firmware has been successfully built with:
- ✅ Debug logging system enabled
- ✅ Port conflict resolved (diagnostic server on 1901)
- ✅ Comprehensive debug output in file operations
- ✅ Memory usage within limits (292KB BSS / 520KB RAM)

## Flashing Instructions

### Method 1: Using picotool (Recommended)
```bash
cd /home/olav/Dokumente/load81
picotool load -f build/load81_picocalc.uf2
```

### Method 2: Manual UF2 Copy
1. Hold BOOTSEL button on Pico2W while connecting USB
2. Pico2W appears as USB drive "RP2350"
3. Copy firmware:
   ```bash
   cp build/load81_picocalc.uf2 /media/olav/RP2350/
   ```
4. Device automatically reboots with new firmware

### Method 3: Using picotool with force
If device is already running:
```bash
picotool load -f build/load81_picocalc.uf2 --force
```

## Verification After Flashing

### 1. Check Splash Screen
**Expected:** "LOAD81 for PicoCalc (debug)"
- The "(debug)" suffix confirms DEBUG_OUTPUT is enabled

### 2. Test Diagnostic Server
```bash
./tools/test_debug_system.sh 192.168.178.122
```

**Expected output:**
- File server responds on port 1900
- Diagnostic server responds on port 1901
- Debug log section shows boot messages

### 3. Verify Debug Log Contains Data
```bash
./tools/get_debug_log.sh 192.168.178.122
```

**Expected output:**
```
=== PicoCalc Boot ===
LOAD81: Starting splash screen
FB_WIDTH=480, FB_HEIGHT=240
...
```

### 4. Test Large File Read with Debug Output
```bash
# In one terminal, monitor debug log
watch -n 1 './tools/get_debug_log.sh 192.168.178.122'

# In another terminal, test large file
./tools/load81r/load81r.py 192.168.178.122 cat /sd/large_file.lua
```

**Expected debug output:**
```
[FS] Reading file: /sd/large_file.lua
[FS] File opened successfully
[FS] File size: 8192 bytes
[FS] Allocated buffer: 8192 bytes
[FS] Read chunk 1: 4096/8192 bytes
[FS] Read chunk 2: 8192/8192 bytes
[FS] File read complete
[CAT] Reading file: /sd/large_file.lua
[CAT] File read successful, size: 8192
[CAT] Sending data...
[SEND] Sending +DATA header
[SEND] Sending 8192 bytes in chunks
[SEND] Chunk 1: 4096 bytes
[SEND] Chunk 2: 4096 bytes
[SEND] Sending +END marker
[SEND] Data transmission complete
```

## Troubleshooting

### Splash screen still doesn't show "(debug)"
- Verify you flashed the correct file: `build/load81_picocalc.uf2`
- Check file timestamp matches recent build
- Try Method 2 (manual UF2 copy) to ensure clean flash

### Debug log still empty after flashing
- Verify splash screen shows "(debug)"
- Check diagnostic server is on port 1901 (not 1900)
- Ensure WiFi is connected (debug messages written during boot)

### picotool not found
```bash
# Install picotool
sudo apt install picotool
```

## What's Next

Once Build 39 is flashed and verified:
1. Debug log will capture all file operations
2. Test large file read (≥8KB)
3. Analyze debug output to identify where timeout occurs
4. Implement targeted fix based on findings

## Build Information

**Build:** 39
**Date:** 2025-12-19
**Changes:**
- Diagnostic server moved to port 1901
- Debug logging system active
- Comprehensive file operation logging

**Firmware Location:** `/home/olav/Dokumente/load81/build/load81_picocalc.uf2`
**Size:** ~556KB text + 292KB BSS