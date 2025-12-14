# SD Card Interface Fix

## Issue
The SD card interface was not working properly - it could not list files on the SD card, even though the reference implementation (picocalc-text-starter v0.14) worked correctly.

## Root Cause
The issue was caused by **explicit initialization** in our code that conflicted with the FAT32 driver's **lazy initialization** design pattern.

### What We Were Doing Wrong

In `src/main.c`, we explicitly called:
```c
fat32_init();
int sd_result = sd_card_init();  // ❌ Explicit call
if (sd_result == 0) {
    fat32_mount();  // ❌ Explicit mount
}
```

This approach doesn't work reliably because:
1. The SD card might not be ready immediately at boot
2. Multiple initialization attempts can cause timing issues
3. It bypasses the driver's internal state management

### The Correct Approach

The FAT32 driver uses **lazy initialization** - it automatically initializes and mounts the SD card when you first try to access files:

```c
fat32_init();  // ✅ Only initialize the subsystem

// Later, when you try to open a file:
fat32_open(&dir, "/load81");  // ✅ Driver handles mounting automatically
```

## Changes Made

### 1. `src/main.c`
- **Removed**: Explicit `sd_card_init()` call
- **Removed**: Explicit `fat32_mount()` call
- **Kept**: Only `fat32_init()` to initialize the subsystem
- **Result**: The driver now handles mounting automatically when files are accessed

### 2. `src/picocalc_menu.c`
- **Removed**: `printf("FAT32 mounted: %d\n", fat32_is_mounted());` debug output
- **Updated**: Comment to clarify that mounting happens automatically
- **Result**: Cleaner code that trusts the driver's lazy initialization

### 3. `src/picocalc_repl.c`
- **Updated**: `lua_sd_reinit()` function to use proper unmount/mount cycle
- **Removed**: Direct `sd_card_init()` call
- **Result**: REPL debug commands work correctly

## How Lazy Initialization Works

1. **During startup**: Only `fat32_init()` is called, which:
   - Initializes SPI hardware via `sd_init()`
   - Starts a timer to detect card removal
   - Does NOT mount the filesystem yet

2. **On first file access**: When you call `fat32_open()` or similar:
   - Driver calls `fat32_is_ready()` internally
   - Which checks if card is present
   - Then calls `fat32_mount()` which internally calls `sd_card_init()`
   - All automatically without explicit intervention

3. **Benefits**:
   - More reliable - handles card insertion timing issues
   - Self-healing - automatically remounts if card is swapped
   - Matches the reference implementation pattern

## Reference
This matches the working implementation in `extern/picocalc-text-start/` where:
- `picocalc.c`: Only calls `fat32_init()` during initialization
- `commands.c`: Simply calls `fat32_open()` and lets the driver handle mounting

## Testing
After flashing the updated firmware:
1. Insert SD card with files in `/load81/` directory
2. Boot the device
3. The menu should now correctly list `.lua` files from the SD card
4. Files can be loaded and executed

## Build Status
✅ Successfully compiled without errors
- Only minor warnings about string truncation (expected)
- Binary ready for flashing: `build/load81_picocalc.uf2`
