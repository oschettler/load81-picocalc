# WiFi Debugging - Build 30

## Problem Summary

Starting with Build 26, WiFi connection has been failing with these symptoms:
- Splash screen stays on for ~1 minute (much longer than normal)
- System shows "Joining" status but never obtains IP address
- Eventually boots to menu without WiFi connection
- No USB debug output visible despite DEBUG_OUTPUT being enabled

## Attempted Fixes (Builds 26-29)

### Build 27
- **Change**: Added `fs_get_file_size()` to eliminate double-read of files
- **Rationale**: Suspected CAT command's double-read was causing memory fragmentation
- **Result**: FAILED - Issue persisted

### Build 28
- **Change**: Removed automatic file server initialization from WiFi connection path
- **Rationale**: Suspected synchronous initialization was blocking WiFi
- **Result**: FAILED - Issue persisted

### Build 29
- **Change**: Enabled USB debug output (DEBUG_OUTPUT=ON)
- **Rationale**: Need diagnostic information to identify root cause
- **Result**: FAILED - Issue persisted, NO debug output visible

## Build 30 - Isolation Test

### Changes
Temporarily removed load81r modules from compilation:
- `src/picocalc_file_server.c` (725 lines)
- `src/picocalc_fs_handler.c` (500 lines)
- `src/picocalc_repl_handler.c` (300 lines)

### Binary Size Comparison
- **Build 29** (with load81r): 556,564 bytes text, 300,288 bytes BSS
- **Build 30** (without load81r): 556,568 bytes text, 300,288 bytes BSS
- **Difference**: Only 4 bytes - modules were already optimized out by linker

### Test Objectives
1. Determine if load81r code is causing WiFi failure
2. Verify USB debug output works when WiFi is functional
3. Establish baseline for comparison

## Diagnostic Observations

### Memory Usage
- RP2350 has 520KB RAM
- BSS usage: 300KB (58% of available RAM)
- Heap fragmentation could be an issue

### USB Debug Output
- `DEBUG_INIT()` calls `stdio_init_all()` with 2-second delay
- USB CDC is non-blocking, should not hang without cable
- No output suggests either:
  - USB enumeration failing
  - System hanging before debug statements execute
  - Debug statements not being reached

### WiFi Connection Timing
- Connection attempt has 30-second timeout
- "Joining" status indicates `CYW43_LINK_JOIN` state
- Never progresses to `CYW43_LINK_UP` (connected with IP)

## Next Steps

### If Build 30 Works
- load81r modules are causing the issue
- Need to identify specific problematic code
- Possible causes:
  - Static initialization issues
  - Memory allocation patterns
  - Interaction with lwIP stack

### If Build 30 Fails
- Issue is NOT related to load81r
- Need to investigate:
  - Changes to other files around Build 26
  - SD card initialization timing
  - WiFi driver configuration
  - Memory corruption from other sources

## Testing Instructions

1. Flash Build 30: `cp build/load81_picocalc.uf2 /media/RPI-RP2/`
2. Connect USB-C cable for debug output
3. Monitor serial: `screen /dev/ttyACM0 115200`
4. Observe:
   - Splash screen duration
   - WiFi connection status
   - Debug output presence
   - Final system state

## Expected Debug Output (if working)

```
=== LOAD81 for PicoCalc Starting ===
Hardware initialized successfully
[WiFi] Initializing CYW43...
[WiFi] CYW43 initialized in station mode
LOAD81: Starting splash screen
[Startup] Checking for /load81/start.lua...
[Startup] Found start.lua, executing...
[WiFi] ============ WiFi Connection Debug ============
[WiFi] SSID: 'YourNetwork'
[WiFi] Starting connection attempt...
[WiFi] Connection attempt completed in XXX ms
[WiFi] ✓ Successfully connected!
[WiFi] IP Address: 192.168.X.X