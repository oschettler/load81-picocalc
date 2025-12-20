# Build 47: Screenshot Feature Implementation

**Date:** 2025-12-19  
**Build Number:** 47  
**Feature:** SSHOT command for capturing PicoCalc display screenshots

## Overview

Added a new `SSHOT` command to the load81r file server and Python client that captures the PicoCalc's 320x320 display and saves it as a PNG file on the Linux side.

## Changes Made

### Server Side (C)

**File: `src/picocalc_file_server.c`**
- Added `cmd_sshot()` handler (lines 600-665)
- Streams 204,800 bytes of RGB565 framebuffer data (320x320 pixels × 2 bytes/pixel)
- Uses same streaming architecture as CAT command for efficient memory usage
- Sends data in 1KB chunks with TCP flow control
- Added SSHOT to command dispatch table

**Key Implementation Details:**
```c
static void cmd_sshot(file_client_t *client, const char *args) {
    const size_t fb_size = FB_WIDTH * FB_HEIGHT * 2;  // 204,800 bytes
    
    // Send +DATA header
    char header[64];
    snprintf(header, sizeof(header), "+DATA %zu\n", fb_size);
    send_response(client, header);
    
    // Stream framebuffer in 1KB chunks
    const uint8_t *fb_data = (const uint8_t *)g_fb.pixels;
    // ... streaming logic with TCP flow control ...
    
    // Send +END marker
    send_response(client, "+END\n");
}
```

### Python Client Side

**File: `tools/load81r/client.py`**
- Added `sshot()` method (lines 311-319)
- Returns raw RGB565 binary data from device

**File: `tools/load81r/commands.py`**
- Added PIL/Pillow import with availability check
- Added `cmd_sshot()` handler (lines 326-387)
- Converts RGB565 to RGB888 format
- Creates PNG using PIL/Pillow
- Includes proper error handling and validation

**RGB565 to RGB888 Conversion:**
```python
# Read RGB565 value (little-endian)
rgb565 = data[i] | (data[i+1] << 8)

# Extract 5-6-5 bit components
r5 = (rgb565 >> 11) & 0x1F
g6 = (rgb565 >> 5) & 0x3F
b5 = rgb565 & 0x1F

# Convert to 8-bit values
r8 = (r5 * 255 + 15) // 31
g8 = (g6 * 255 + 31) // 63
b8 = (b5 * 255 + 15) // 31
```

**File: `tools/load81r/shell.py`**
- Added sshot to command imports
- Added sshot to tab completion
- Added command dispatcher entry

## Usage

### Interactive Shell
```bash
$ python3 tools/load81r/load81r.py picocalc
load81r:/> sshot screenshot.png
Capturing screenshot...
Converting RGB565 to PNG...
Screenshot saved to screenshot.png
```

### Command Line
```bash
$ python3 tools/load81r/load81r.py picocalc sshot menu.png
```

### Help
```bash
load81r:/> help sshot
sshot FILENAME
  Capture screenshot from PicoCalc display and save as PNG
  Requires PIL/Pillow: pip install pillow
```

## Dependencies

The screenshot feature requires PIL/Pillow on the Python side:
```bash
pip install pillow
```

If PIL is not installed, the command will display a helpful error message.

## Technical Details

### Framebuffer Format
- **Resolution:** 320×320 pixels
- **Format:** RGB565 (16-bit color)
- **Size:** 204,800 bytes (320 × 320 × 2)
- **Memory Location:** `g_fb.pixels` in `picocalc_framebuffer.h`

### Data Transfer
- Uses same streaming architecture as CAT command
- Sends data in 1KB chunks
- Implements TCP flow control with `tcp_sndbuf()` checks
- Flushes every 4KB to prevent buffer buildup
- Total transfer time: ~1-2 seconds depending on network

### Color Conversion
RGB565 format packs colors into 16 bits:
- Red: 5 bits (bits 11-15)
- Green: 6 bits (bits 5-10)
- Blue: 5 bits (bits 0-4)

Conversion to RGB888 (24-bit) uses proper scaling to maintain color accuracy.

## Testing

1. **Build and flash firmware:**
   ```bash
   ./increment_build.sh
   cp build/load81_picocalc.uf2 /media/RPI-RP2/
   ```

2. **Connect to device:**
   ```bash
   python3 tools/load81r/load81r.py picocalc
   ```

3. **Capture screenshot:**
   ```bash
   load81r:/> sshot test.png
   ```

4. **Verify PNG file:**
   ```bash
   file test.png
   # Should show: PNG image data, 320 x 320, 8-bit/color RGB
   ```

## Performance

- **Memory usage:** Minimal - streams data without buffering entire framebuffer
- **Transfer time:** ~1-2 seconds for 200KB
- **CPU impact:** Negligible - uses existing framebuffer data
- **Network impact:** Same as reading a 200KB file

## Future Enhancements

Possible improvements:
1. Add optional compression (JPEG format for smaller files)
2. Support for cropping/region capture
3. Batch screenshot capture for animation
4. Real-time streaming for remote display

## Related Files

- `src/picocalc_file_server.c` - Server implementation
- `src/picocalc_framebuffer.h` - Framebuffer definitions
- `tools/load81r/client.py` - Protocol client
- `tools/load81r/commands.py` - Command handlers
- `tools/load81r/shell.py` - Interactive shell

## Build Information

- **Build Number:** 47
- **Previous Build:** 46 (large file streaming fix)
- **Firmware Size:** ~450KB
- **Compilation:** Successful with no errors
- **Warnings:** Standard strncpy truncation warnings (expected)