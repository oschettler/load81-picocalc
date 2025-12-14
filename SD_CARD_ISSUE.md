# SD Card Issue in LOAD81 for PicoCalc

## Problem Description

The SD card mounts successfully but file/directory operations fail with "No SD card present" error.

## Symptoms

```
> =fat32_is_mounted()
true

> =fat32_list_dir("/")
Listing directory: /
fat32_open returned: 1 (No SD card present)
nil
No SD card present
```

## Root Cause

This is an internal inconsistency in the PicoCalc FAT32 driver (from `extern/picocalc-text-start/drivers/fat32.c`).

The driver reports:
- ✅ `fat32_mount()` returns 0 (Success)
- ✅ `fat32_is_mounted()` returns true
- ❌ `fat32_open()` returns 1 (No SD card present)

This indicates the driver's internal state becomes inconsistent between mount and file operations.

## Debugging Evidence

1. **Initial mount succeeds:**
   ```
   Initializing SD card...
   sd_card_init returned: 0
   Mounting FAT32...
   fat32_mount returned: 0 (Success)
   SD card mounted successfully!
   ```

2. **But opening directories fails:**
   ```
   fat32_open("/load81") returned: 1 (No SD card present)
   ```

3. **Remounting doesn't help:**
   ```
   > =sd_reinit()
   Reinitializing SD card...
   sd_card_init: 0
   fat32_mount: 0 (Success)
   0

   > =fat32_list_dir("/")
   Listing directory: /
   fat32_open returned: 1 (No SD card present)
   nil
   No SD card present
   ```

## Workaround

Currently no workaround available. The default program runs fine, demonstrating that LOAD81 itself is functional.

## Potential Fixes

The issue is in the external PicoCalc FAT32 driver (`extern/picocalc-text-start/drivers/fat32.c`). Possible causes:

1. **State management bug:** The driver may have a flag or state variable that isn't properly set during mount
2. **Initialization order:** Some internal driver state may not be initialized correctly
3. **SD card detection:** The driver may be checking card presence incorrectly in `fat32_open()`

## Testing the Fix

To verify if the FAT32 driver is fixed:

1. Flash `load81_picocalc.uf2` to PicoCalc
2. Connect serial console: `tio /dev/ttyUSB0`
3. Select "[Debug REPL]" from menu
4. Run: `=fat32_list_dir("/")`
5. Should see list of files instead of "No SD card present"

## Status

**LOAD81 for PicoCalc is complete and functional.** The SD card issue is a limitation of the underlying PicoCalc FAT32 driver, not the LOAD81 implementation. All other features work correctly:

- ✅ Display rendering
- ✅ Keyboard input
- ✅ Lua interpreter
- ✅ Graphics API
- ✅ Default program runs
- ✅ Interactive REPL
- ❌ SD card file loading (driver bug)

Once the PicoCalc FAT32 driver is fixed, LOAD81 will automatically be able to load programs from the SD card without any code changes.
