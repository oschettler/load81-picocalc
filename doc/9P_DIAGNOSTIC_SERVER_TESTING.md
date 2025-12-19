# 9P Diagnostic Server Testing Guide

## Overview

This guide covers testing the diagnostic NEX server on port 1900 to determine why the 9P server on port 564 is not accessible from Linux clients.

## Build Information

**Current Build:** 3 (with diagnostic server)
**Firmware:** `build/load81_picocalc.uf2`
**Diagnostic Server Port:** 1900
**9P Server Port:** 564

## Flashing Instructions

1. Connect PicoCalc via USB while holding BOOTSEL button
2. Copy firmware to mounted drive:
   ```bash
   cp build/load81_picocalc.uf2 /media/$USER/RPI-RP2/
   ```
3. Device will automatically reboot and start firmware

## Testing Sequence

### Step 1: Verify WiFi Connection

1. Power on PicoCalc
2. Wait for WiFi connection (status shows "connected")
3. Note the IP address displayed on screen (e.g., 192.168.178.122)
4. Verify build number shows "Build 3" in lower right corner

### Step 2: Check Server Status via Lua

In the PicoCalc REPL, run:
```lua
wifi.p9_info()
```

Expected output:
```
9P: Active
Running: Yes
Clients: 0
Diag: Active
Port: 1900
```

### Step 3: Test Diagnostic Server from Linux

#### Test 3a: Port Connectivity Check
```bash
# Replace with your PicoCalc's actual IP
PICOCALC_IP=192.168.178.122

# Test if port 1900 is reachable
nc -zv $PICOCALC_IP 1900
```

**Expected Results:**
- **Success:** `Connection to 192.168.178.122 1900 port [tcp/*] succeeded!`
- **Failure:** `No route to host` or `Connection refused`

#### Test 3b: Fetch Status via NEX Protocol
```bash
# Method 1: Using netcat
echo "/status" | nc $PICOCALC_IP 1900

# Method 2: Using telnet
telnet $PICOCALC_IP 1900
# Then type: /status
# Press Ctrl+] then type 'quit' to exit

# Method 3: Using curl (if server responds to HTTP)
curl http://$PICOCALC_IP:1900/status
```

**Expected Response:**
```
PicoCalc Diagnostic Server
==========================
9P Server Status:
  Active: Yes
  Running: Yes
  Clients: 0
  Port: 564

Diagnostic Server:
  Port: 1900
  Active: Yes

WiFi Status:
  Connected: Yes
  IP: 192.168.178.122
```

### Step 4: Compare with 9P Server Port

```bash
# Test 9P server port
nc -zv $PICOCALC_IP 564
```

**Expected Results:**
- **Success:** Connection succeeds (problem is with 9P protocol, not network)
- **Failure:** Connection fails (problem is with incoming connections in general)

### Step 5: Network Diagnostics

#### Check ARP Table
```bash
# Verify PicoCalc is in ARP table
arp -a | grep $PICOCALC_IP
```

Expected: Entry should exist showing MAC address

#### Check Routing
```bash
# Verify route to PicoCalc
ip route get $PICOCALC_IP
```

Expected: Should show route via local network interface

#### Packet Capture
```bash
# Capture packets to/from PicoCalc (requires root)
sudo tcpdump -i wlan0 host $PICOCALC_IP and port 1900
# In another terminal, try connecting:
nc -zv $PICOCALC_IP 1900
```

Look for:
- SYN packets being sent
- SYN-ACK responses (or lack thereof)
- RST packets indicating connection refused

## Diagnostic Outcomes

### Outcome A: Both Ports Work
**Symptoms:**
- Port 1900 connects successfully ✓
- Port 564 connects successfully ✓

**Diagnosis:** 9P protocol implementation issue, not network connectivity

**Next Steps:**
1. Test 9P protocol handshake
2. Check 9P message parsing
3. Verify 9P version negotiation

### Outcome B: Port 1900 Works, Port 564 Doesn't
**Symptoms:**
- Port 1900 connects successfully ✓
- Port 564 connection fails ✗

**Diagnosis:** Port-specific issue or 9P server binding problem

**Next Steps:**
1. Check if port 564 requires special privileges
2. Verify `tcp_bind()` return value in 9P server
3. Check if lwIP has port restrictions
4. Try alternative port (e.g., 5640)

### Outcome C: Neither Port Works
**Symptoms:**
- Port 1900 connection fails ✗
- Port 564 connection fails ✗

**Diagnosis:** General incoming connection problem

**Possible Causes:**
1. **lwIP Configuration Issue**
   - Firewall rules blocking incoming connections
   - TCP stack not properly initialized
   - Network interface not accepting connections

2. **WiFi Driver Issue**
   - CYW43 driver not fully initialized
   - WiFi in client-only mode (no server capability)
   - Driver bug preventing incoming connections

3. **Network Configuration**
   - Router still blocking despite settings
   - AP isolation at hardware level
   - VLAN or subnet isolation

**Next Steps:**
1. Check lwIP configuration in CMakeLists.txt
2. Verify CYW43 initialization sequence
3. Test with different router/network
4. Check if outgoing connections still work (nex.lua)

### Outcome D: Intermittent Connectivity
**Symptoms:**
- Sometimes connects, sometimes fails
- Connection drops after short time

**Diagnosis:** Timing or resource issue

**Possible Causes:**
1. Core 1 not polling lwIP frequently enough
2. Memory exhaustion
3. Buffer overflow
4. Race condition in server initialization

## Additional Debugging

### Enable Debug Output

If needed, rebuild with debug output:
```bash
cd build
cmake -DDEBUG_OUTPUT=ON ..
make -j$(nproc)
```

Then connect USB cable and monitor output:
```bash
# On Linux
screen /dev/ttyACM0 115200

# Or use minicom
minicom -D /dev/ttyACM0 -b 115200
```

### Check Server Initialization Logs

With debug output enabled, look for:
```
[DIAG] Server starting on port 1900
[DIAG] Bind result: 0 (0 = success)
[DIAG] Listen result: <pcb_address>
[DIAG] Server ready, waiting for connections
```

### Monitor Connection Attempts

Debug output will show:
```
[DIAG] Accept callback triggered
[DIAG] New client connected from <ip>:<port>
[DIAG] Received <n> bytes
[DIAG] Sending response...
```

## Testing Checklist

- [ ] Firmware flashed successfully
- [ ] WiFi connected and IP displayed
- [ ] Build number shows "Build 3"
- [ ] `wifi.p9_info()` shows both servers active
- [ ] Port 1900 connectivity tested
- [ ] Port 564 connectivity tested
- [ ] Diagnostic server response received
- [ ] Network diagnostics completed
- [ ] Outcome determined
- [ ] Next steps identified

## Common Issues and Solutions

### Issue: "No route to host"
**Cause:** Network routing problem or firewall
**Solution:** 
- Verify IP address is correct
- Check router settings
- Try from different device on same network

### Issue: "Connection refused"
**Cause:** Server not listening on port
**Solution:**
- Verify server started (`wifi.p9_info()`)
- Check `tcp_bind()` return value
- Verify port number is correct

### Issue: Connection hangs
**Cause:** Server not responding to TCP handshake
**Solution:**
- Check Core 1 is running
- Verify lwIP polling is active
- Check for deadlocks or infinite loops

### Issue: Response truncated
**Cause:** Buffer size or TCP window issue
**Solution:**
- Check response buffer size (currently 512 bytes)
- Verify TCP send is completing
- Check for memory corruption

## Next Steps After Testing

Based on test results, proceed to:
1. **If network works:** Debug 9P protocol implementation
2. **If network fails:** Debug lwIP/CYW43 configuration
3. **If intermittent:** Add more logging and timing analysis

## Contact Information

For issues or questions:
- Check existing documentation in `doc/` directory
- Review 9P implementation in `src/picocalc_9p*.c`
- Review diagnostic server in `src/picocalc_diag_server.c`