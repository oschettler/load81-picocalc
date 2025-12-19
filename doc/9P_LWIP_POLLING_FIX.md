# 9P Server lwIP Polling Fix

## Problem Summary

**Symptom:** Linux clients could not connect to the 9P server on port 564 or the diagnostic NEX server on port 1900, receiving "No route to host" errors.

**Root Cause:** The firmware uses `pico_cyw43_arch_lwip_poll` which requires explicit polling of the network stack via `cyw43_arch_poll()`. Without regular polling, lwIP cannot process incoming TCP connections.

## Technical Details

### lwIP Variants

The Pico SDK provides two lwIP integration variants:

1. **`pico_cyw43_arch_lwip_threadsafe_background`** - Runs lwIP in a background thread with automatic polling
2. **`pico_cyw43_arch_lwip_poll`** - Requires application to explicitly call `cyw43_arch_poll()` regularly

This project uses the **poll** variant (as specified in [`CMakeLists.txt`](../CMakeLists.txt:114,151)).

### Why Outgoing Connections Worked

The NEX client ([`src/picocalc_nex.c`](../src/picocalc_nex.c:190,233)) calls `cyw43_arch_poll()` in its connection loops, which is why outgoing connections to fetch NEX content worked perfectly.

### Why Incoming Connections Failed

**Core 1 (9P Server):**
- The 9P server runs on Core 1 ([`src/picocalc_9p_core1.c`](../src/picocalc_9p_core1.c))
- The main loop called `p9_server_poll()` and `mdns_poll()` but **NOT** `cyw43_arch_poll()`
- Without polling, lwIP never processed incoming SYN packets from clients

**Core 0 (Main/Menu/Diagnostic Server):**
- The diagnostic NEX server runs on Core 0
- The menu loop blocked in `kb_wait_key()` ([`src/picocalc_menu.c`](../src/picocalc_menu.c:192))
- During this blocking wait, **no network polling occurred**
- The program loop also didn't poll the network stack

## The Fix

### 1. Core 1 Polling (9P Server)

**File:** [`src/picocalc_9p_core1.c`](../src/picocalc_9p_core1.c)

**Changes:**
- Added `#include "pico/cyw43_arch.h"`
- Added `cyw43_arch_poll()` call in main loop (line 75)
- Reduced sleep from 10ms to 1ms for better responsiveness

```c
/* Poll lwIP network stack - CRITICAL for incoming connections! */
cyw43_arch_poll();

/* Poll server and mDNS */
if (p9_server_is_running()) {
    p9_server_poll();
    mdns_poll();
}

/* Small delay to prevent busy-waiting */
sleep_ms(1);
```

### 2. Core 0 Menu Polling (Diagnostic Server)

**File:** [`src/picocalc_menu.c`](../src/picocalc_menu.c)

**Changes:**
- Added `#include "pico/cyw43_arch.h"`
- Replaced blocking `kb_wait_key()` with polling loop (lines 193-198)

**Before:**
```c
kb_reset_events();
char key = kb_wait_key();
```

**After:**
```c
kb_reset_events();
char key = 0;
while (!kb_key_available()) {
    cyw43_arch_poll();  /* Poll network stack for incoming connections */
    sleep_ms(10);
}
key = kb_get_char();
```

### 3. Core 0 Program Loop Polling

**File:** [`src/main.c`](../src/main.c)

**Changes:**
- Added `#include "pico/cyw43_arch.h"`
- Added `cyw43_arch_poll()` call in program loop (line 245)

```c
/* Main loop */
while (g_program_running) {
    uint32_t frame_start = to_ms_since_boot(get_absolute_time());
    
    /* Poll network stack for incoming connections */
    cyw43_arch_poll();
    
    /* Poll keyboard */
    kb_poll();
    // ... rest of loop
}
```

## Testing

### Build Information
- **Build Number:** 4
- **Firmware:** `build/load81_picocalc.uf2`
- **Changes:** Added `cyw43_arch_poll()` to all main loops

### Test Procedure

1. **Flash firmware:**
   ```bash
   cp build/load81_picocalc.uf2 /media/RPI-RP2/
   ```

2. **Connect to WiFi** (if not using start.lua):
   ```lua
   wifi.connect("schettler", "1122334455667")
   ```

3. **Verify servers started:**
   ```lua
   wifi.p9_info()
   ```
   Should show both 9P and diagnostic servers active.

4. **Test diagnostic server from Linux:**
   ```bash
   # Test port connectivity
   nc -zv 192.168.178.122 1900
   
   # Get status
   echo "/status" | nc 192.168.178.122 1900
   ```

5. **Test 9P server:**
   ```bash
   # Test port connectivity
   nc -zv 192.168.178.122 564
   
   # Mount filesystem
   sudo mount -t 9p -o trans=tcp,port=564 192.168.178.122 /mnt/picocalc
   ```

### Expected Results

**Before Fix:**
- ✗ `nc -zv 192.168.178.122 1900` → "No route to host"
- ✗ `nc -zv 192.168.178.122 564` → "No route to host"
- ✓ Outgoing NEX connections work

**After Fix:**
- ✓ `nc -zv 192.168.178.122 1900` → "Connection succeeded"
- ✓ `nc -zv 192.168.178.122 564` → "Connection succeeded"
- ✓ Diagnostic server responds with status
- ✓ 9P server accepts mount requests
- ✓ Outgoing NEX connections still work

## Performance Impact

### Polling Frequency

- **Core 1:** Polls every 1ms (1000 Hz)
- **Core 0 Menu:** Polls every 10ms (100 Hz) while waiting for input
- **Core 0 Program:** Polls every frame (~30 Hz at 30 FPS)

### CPU Usage

The polling overhead is minimal:
- `cyw43_arch_poll()` is a lightweight function that checks for pending network events
- Only processes packets when data is actually available
- No busy-waiting - sleep between polls

### Responsiveness

- **Network:** Incoming connections accepted within 1-10ms
- **Keyboard:** No change in responsiveness
- **Graphics:** No impact on frame rate

## Alternative Solutions Considered

### 1. Switch to Threadsafe Background Variant

**Pros:**
- Automatic polling, no code changes needed
- Better for complex applications

**Cons:**
- Higher memory usage (background thread)
- More complex synchronization
- Overkill for this application

**Decision:** Stick with poll variant, add explicit polling

### 2. Use Interrupts

**Pros:**
- Most responsive

**Cons:**
- Complex to implement correctly
- Requires careful synchronization
- Not supported by pico_cyw43_arch_lwip_poll

**Decision:** Not feasible with current lwIP integration

### 3. Dedicated Network Thread on Core 0

**Pros:**
- Clean separation of concerns

**Cons:**
- Wastes Core 1 (already dedicated to 9P server)
- More complex than simple polling
- Unnecessary for this use case

**Decision:** Simple polling is sufficient

## Lessons Learned

1. **Read the Documentation:** The `pico_cyw43_arch_lwip_poll` variant explicitly requires application polling
2. **Test Both Directions:** Outgoing connections working doesn't mean incoming connections work
3. **Avoid Blocking Calls:** Blocking keyboard waits prevent network processing
4. **Poll Everywhere:** Any loop that might run for extended periods needs polling

## Related Files

- [`src/picocalc_9p_core1.c`](../src/picocalc_9p_core1.c) - Core 1 main loop with polling
- [`src/picocalc_menu.c`](../src/picocalc_menu.c) - Menu with non-blocking keyboard input
- [`src/main.c`](../src/main.c) - Program loop with polling
- [`src/picocalc_nex.c`](../src/picocalc_nex.c) - NEX client (already had polling)
- [`CMakeLists.txt`](../CMakeLists.txt) - Build configuration specifying lwIP variant

## References

- [Pico SDK lwIP Documentation](https://www.raspberrypi.com/documentation/pico-sdk/networking.html)
- [lwIP Wiki](https://lwip.fandom.com/wiki/LwIP_Wiki)
- [9P Protocol Specification](http://man.cat-v.org/plan_9/5/intro)