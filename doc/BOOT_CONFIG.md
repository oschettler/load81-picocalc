# Boot Configuration for PicoCalc

## Standalone Boot (Default)

By default, the PicoCalc now boots **without requiring a USB-C cable**. Debug output is disabled, allowing the device to boot independently.

The default build configuration is:
```bash
cd build
cmake ..
make -j4
```

This creates a `load81_picocalc.uf2` file that will boot standalone.

## Debug Mode (with USB-C cable)

To enable debug output over UART (requires USB-C connection), rebuild with the `DEBUG_OUTPUT` option:

```bash
cd build
cmake -DDEBUG_OUTPUT=ON ..
make -j4
```

This will:
- Enable all `DEBUG_PRINTF()` statements
- Initialize stdio for UART communication
- Output diagnostic information via the serial console

### Viewing Debug Output

With DEBUG_OUTPUT enabled, connect to the serial console:

```bash
tio /dev/ttyUSB0
```

You'll see diagnostic messages like:
```
=== LOAD81 for PicoCalc Starting ===
Initializing SD card subsystem...
LOAD81: Starting splash screen
FB_WIDTH=320, FB_HEIGHT=320
Hardware initialized successfully
[Startup] Checking for /load81/start.lua...
Loading programs from /load81/ directory...
```

## Implementation Details

The conditional debug output is controlled by the `DEBUG_OUTPUT` compile-time flag:

### In `src/debug.h`:
```c
#ifdef DEBUG_OUTPUT
    #include <stdio.h>
    #define DEBUG_INIT() stdio_init_all()
    #define DEBUG_PRINTF(...) printf(__VA_ARGS__)
#else
    #define DEBUG_INIT() ((void)0)
    #define DEBUG_PRINTF(...) ((void)0)
#endif
```

### Modified Files:
- `src/main.c` - Replaced all `printf()` with `DEBUG_PRINTF()`
- `src/picocalc_menu.c` - Debug output for file operations
- `src/picocalc_editor.c` - Debug output for editor operations
- `src/picocalc_wifi.c` - Debug output for WiFi operations
- `src/picocalc_nex.c` - Debug output for NEX protocol
- `src/picocalc_repl.c` - Debug output for REPL operations

## Switching Between Configurations

To switch back to standalone mode after building with DEBUG_OUTPUT:

```bash
cd build
cmake -DDEBUG_OUTPUT=OFF ..
make -j4
```

Or simply rebuild without the flag:

```bash
cd build
cmake ..
make -j4
```

## Benefits

**Standalone Mode (DEBUG_OUTPUT=OFF):**
- ✅ Boots without USB cable
- ✅ Smaller binary size
- ✅ Faster boot time (no stdio initialization)
- ✅ Perfect for production use

**Debug Mode (DEBUG_OUTPUT=ON):**
- ✅ Full diagnostic output
- ✅ Easier troubleshooting
- ✅ File operation logging
- ✅ Development and testing
