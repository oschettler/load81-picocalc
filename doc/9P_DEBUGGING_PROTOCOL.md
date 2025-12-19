# 9P Protocol Debugging Guide

## Current Status (Build 5)

**Symptom:** Mount fails with "Der Superblock von 192.168.178.122 konnte nicht gelesen werden" (Cannot read superblock)
- No dmesg output (connection not reaching kernel)
- Diagnostic server (port 1900) works fine
- Device stays responsive

**Hypothesis:** TCP connection to port 564 is not being established or accepted.

## Debugging Steps

### 1. Verify Port 564 is Listening

From Linux client:
```bash
# Check if port 564 is open
nc -zv 192.168.178.122 564

# Try to connect and see what happens
nc 192.168.178.122 564
# (Press Ctrl+C after a few seconds)
```

**Expected:** Connection should succeed
**If fails:** Port 564 is not listening or firewall blocking

### 2. Compare with Working Diagnostic Server

```bash
# This works (port 1900)
echo "/status" | nc 192.168.178.122 1900

# Try raw connection to 9P port
nc 192.168.178.122 564
# Type some random characters and press Enter
# Should get some response or connection close
```

### 3. Test with tcpdump

On Linux client (requires root):
```bash
# Capture traffic to port 564
sudo tcpdump -i any -n host 192.168.178.122 and port 564 -X

# In another terminal, try mount
sudo mount -t 9p -o trans=tcp,port=564 192.168.178.122 /mnt/picocalc
```

**Look for:**
- SYN packets being sent
- SYN-ACK responses (or lack thereof)
- RST packets (connection refused)

### 4. Test 9P Protocol Manually

Create a test script to send 9P version message:
```bash
#!/bin/bash
# Send Tversion message manually
# Format: size[4] type[1] tag[2] msize[4] version[s]

# Tversion: size=19, type=100, tag=65535, msize=8192, version="9P2000.u"
printf '\x13\x00\x00\x00\x64\xff\xff\x00\x20\x00\x00\x08\x00\x39\x50\x32\x30\x30\x30\x2e\x75' | nc 192.168.178.122 564 | xxd
```

**Expected:** Should receive Rversion response
**Format:** size[4] type[1]=101 tag[2] msize[4] version[s]

### 5. Enable Debug Output

We need to add logging to see what's happening. The firmware has `DEBUG_OUTPUT` disabled by default.

## Potential Issues

### Issue 1: Port 564 Not Binding

**Check in code:**
- [`src/picocalc_9p.c`](src/picocalc_9p.c:78) - `tcp_bind()` on port 564
- May fail silently if port already in use or permission issue

**Solution:** Add error logging or try different port

### Issue 2: Accept Callback Not Firing

**Check in code:**
- [`src/picocalc_9p.c`](src/picocalc_9p.c:96) - `tcp_accept()` callback
- [`src/picocalc_9p.c`](src/picocalc_9p.c:165) - `p9_tcp_accept()` function

**Solution:** Add logging to accept callback

### Issue 3: lwIP Configuration

**Possible problems:**
- TCP backlog too small
- Memory allocation issues
- lwIP not processing port 564 for some reason

### Issue 4: Firewall or Router

**Less likely but possible:**
- Fritz!Box blocking port 564
- Client firewall blocking outgoing to 564

## Next Steps

### Option A: Add Comprehensive Logging

Create a debug build with logging enabled:
1. Add `#define DEBUG_9P_SERVER 1` to enable logging
2. Log every TCP event (bind, listen, accept, recv, send)
3. Log every 9P message received and sent
4. Flash and test

### Option B: Test with Simple TCP Echo

Temporarily replace 9P server with simple echo server on port 564:
1. Accept connection
2. Echo back any received data
3. Verify basic TCP works on port 564

### Option C: Use Different Port

Try port 5640 instead of 564:
1. Change `P9_SERVER_PORT` to 5640
2. Mount with `-o port=5640`
3. See if port number matters

### Option D: Packet Capture on Device

If possible, capture packets on the PicoCalc side to see if:
- SYN packets are arriving
- Server is sending SYN-ACK
- Connection is being established

## Code Locations to Investigate

1. **Server Initialization:**
   - [`src/picocalc_9p.c`](src/picocalc_9p.c:59-102) - `p9_server_start()`
   - Check return values from `tcp_bind()` and `tcp_listen()`

2. **Accept Callback:**
   - [`src/picocalc_9p.c`](src/picocalc_9p.c:165-208) - `p9_tcp_accept()`
   - Add logging to see if this is ever called

3. **Core 1 Loop:**
   - [`src/picocalc_9p_core1.c`](src/picocalc_9p_core1.c:48-90) - Main loop
   - Verify server is actually started

4. **WiFi Callback:**
   - [`src/picocalc_wifi.c`](src/picocalc_wifi.c:149-163) - Server start on WiFi connect
   - Verify this is being called

## Comparison: Diagnostic Server vs 9P Server

Both use identical TCP setup pattern:
- `tcp_new()` → `tcp_bind()` → `tcp_listen()` → `tcp_accept()`
- Diagnostic server works, 9P server doesn't
- **Key difference:** Port number (1900 vs 564)

**Hypothesis:** Something specific to port 564 is failing.

## Recommended Immediate Action

Run these commands and report results:

```bash
# 1. Test port connectivity
nc -zv 192.168.178.122 564
nc -zv 192.168.178.122 1900

# 2. Try raw connection
timeout 5 nc 192.168.178.122 564 && echo "Connected" || echo "Failed"

# 3. Compare with working port
echo "/status" | nc 192.168.178.122 1900

# 4. Check if anything responds on 564
echo "test" | nc -w 2 192.168.178.122 564 | xxd
```

Report the output of all four commands.