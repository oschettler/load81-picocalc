# Debugging LOAD81 on PicoCalc

## Current Status
The UF2 file builds successfully but displays garbled text on the PicoCalc screen. Debug output has been enabled to diagnose the issue.

## Debug Steps

### 1. Flash the Debug Build
Flash the new debug-enabled UF2 file:
```bash
cp /home/olav/Dokumente/load81/build/load81_picocalc.uf2 /media/<username>/RPI-RP2/
```

### 2. Connect to Serial Console
```bash
tio /dev/ttyUSB0
```

### 3. View Debug Output
After flashing and connecting to serial, press reset on the Pico. You should see:
```
=== LOAD81 for PicoCalc Starting ===
Hardware initialized successfully
LOAD81: Starting splash screen
FB_WIDTH=320, FB_HEIGHT=320
Background filled
Drawing text at (60, 180)
Presenting framebuffer
```

## Known Issues

### Garbled Display
**Symptoms:** Text appears as "   A 81" instead of "LOAD81 for PicoCalc"

**Possible Causes:**
1. **Y-coordinate inversion**: PicoCalc LCD may have Y=0 at bottom instead of top
2. **Font rendering**: Character data might not be loading correctly
3. **Framebuffer sync**: RGB565 conversion or buffer transfer issue

### No Keyboard Response
**Symptoms:** Cursor keys don't navigate menu

**Possible Causes:**
1. Keyboard polling not working correctly
2. Key mapping issues between PicoCalc keyboard and our code

## Debug Output Analysis

Look for these in the serial output:
- Does `FB_WIDTH` and `FB_HEIGHT` show 320x320?
- Does "Drawing text at (60, 180)" appear?
- Are there any error messages?
- Does the program reach "Presenting framebuffer"?

## Next Steps

Based on the serial output, we can:
1. Fix coordinate system if Y is inverted
2. Debug font rendering if characters are wrong
3. Check framebuffer-to-LCD transfer if nothing displays
4. Add more detailed logging to pinpoint the issue
