# 9P Server Debugging Guide

This guide helps diagnose and fix issues with the 9P filesystem server on PicoCalc.

---

## Quick Diagnostic Steps

### 1. Check WiFi Connection and IP Address

After flashing the new firmware, the menu should display the IP address in the top-right corner when connected:

```
Expected: 192.168.x.x (in green)
Problem: "Joining" or "Disconn" (in blue/gray)
```

**If you see "Joining":** WiFi is connecting but hasn't obtained an IP yet. Wait a few seconds.

**If you see the IP address:** WiFi is connected successfully.

### 2. Check 9P Server Status via Lua REPL

From the PicoCalc REPL, run:

```lua
-- Check WiFi status
print(wifi.status())
print(wifi.ip())

-- Check 9P server status (NEW in this build)
status = wifi.p9_status()
print("Active:", status.active)
print("Running:", status.running)
print("Clients:", status.clients)
print("Port:", status.port)
```

**Expected output when working:**
```
Active: true
Running: true
Clients: 0
Port: 564
```

**If Active/Running are false:** The server didn't start. This could mean:
- Core 1 didn't launch properly
- Server initialization failed
- WiFi wasn't connected when server tried to start

### 3. Test Network Connectivity from Linux

Before trying to mount, verify basic network connectivity:

```bash
# Ping the PicoCalc
ping 192.168.178.122

# Check if port 564 is open
nc -zv 192.168.178.122 564
# or
telnet 192.168.178.122 564
```

**Expected:** Ping should work, port 564 should be open/reachable.

**If ping fails:** Network routing issue, firewall, or wrong IP.

**If port 564 is closed:** 9P server isn't listening.

---

## Common Issues and Solutions

### Issue 1: "No route to host" Error

**Symptoms:**
```
mount: /mnt/picocalc: mount(2)-Systemaufruf ist fehlgeschlagen: Keine Route zum Zielrechner.
dmesg: 9pnet_fd: p9_fd_create_tcp: problem connecting socket to 192.168.178.122
```

**Diagnosis:**
This means the Linux client cannot establish a TCP connection to the PicoCalc on port 564.

**Possible Causes:**

1. **9P Server Not Running**
   - Check via Lua: `wifi.p9_status()`
   - Solution: Reconnect WiFi to trigger server start

2. **Firewall Blocking Connection**
   - Check Linux firewall: `sudo iptables -L`
   - Solution: Allow outgoing connections to port 564

3. **Wrong IP Address**
   - Verify IP on PicoCalc menu screen
   - Solution: Use correct IP address

4. **Network Routing Issue**
   - Check if devices are on same subnet
   - Solution: Ensure both devices can ping each other

### Issue 2: Server Shows as "Not Running"

**If `wifi.p9_status()` shows `running: false`:**

**Possible Causes:**

1. **WiFi Not Connected**
   - Server only starts after successful WiFi connection
   - Solution: Run `wifi.connect("SSID", "password")` again

2. **Core 1 Not Launched**
   - Core 1 should launch during boot
   - Solution: Reboot PicoCalc (power cycle)

3. **Port Already in Use**
   - Another process might be using port 564
   - Solution: Reboot PicoCalc

4. **lwIP Stack Issue**
   - TCP/IP stack might not be initialized
   - Solution: Reconnect WiFi

**Recovery Steps:**
```lua
-- Disconnect and reconnect WiFi
wifi.disconnect()
sleep(2)
wifi.connect("YourSSID", "YourPassword")
sleep(5)

-- Check status again
status = wifi.p9_status()
print("Running:", status.running)
```

### Issue 3: Mount Command Syntax Errors

**Correct mount syntax:**

```bash
# Method 1: Direct IP (most reliable)
sudo mount -t 9p -o trans=tcp,port=564 192.168.178.122 /mnt/picocalc

# Method 2: With additional options
sudo mount -t 9p -o trans=tcp,port=564,version=9p2000.u 192.168.178.122 /mnt/picocalc

# Method 3: With debug output
sudo mount -t 9p -o trans=tcp,port=564,debug=1 192.168.178.122 /mnt/picocalc
```

**Common mistakes:**
- Missing `sudo` (mount requires root)
- Wrong port number (must be 564)
- Mount point doesn't exist (create with `sudo mkdir -p /mnt/picocalc`)
- Using hostname instead of IP (mDNS might not work yet)

---

## Advanced Debugging

### Enable Debug Output

To see detailed debug messages, rebuild firmware with debug output enabled:

```bash
cd build
cmake .. -DDEBUG_OUTPUT=ON
make -j$(nproc)
```

Then connect via USB and monitor output:
```bash
# Linux
sudo screen /dev/ttyACM0 115200

# Or use minicom
sudo minicom -D /dev/ttyACM0 -b 115200
```

### Check dmesg for Kernel Messages

```bash
# View recent kernel messages
sudo dmesg | tail -50

# Watch for new messages in real-time
sudo dmesg -w
```

Look for messages containing:
- `9pnet`: 9P protocol messages
- `9p`: Filesystem messages
- Connection errors or timeouts

### Test with netcat

Manually test if the server accepts connections:

```bash
# Try to connect to port 564
nc 192.168.178.122 564
```

**Expected:** Connection should be accepted (you'll see a blank screen, press Ctrl+C to exit)

**If connection refused:** Server isn't listening

**If connection timeout:** Network/firewall issue

### Verify 9P Protocol Handshake

Use a simple Python script to test the 9P protocol:

```python
#!/usr/bin/env python3
import socket

# Connect to 9P server
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('192.168.178.122', 564))

# Send Tversion message (simplified)
# Format: size(4) + type(1) + tag(2) + msize(4) + version_len(2) + version
version = b'9P2000.u'
msg = bytearray()
msg += (19 + len(version)).to_bytes(4, 'little')  # size
msg += b'\x64'  # Tversion = 100
msg += (0).to_bytes(2, 'little')  # tag = 0
msg += (8192).to_bytes(4, 'little')  # msize
msg += len(version).to_bytes(2, 'little')  # version length
msg += version

sock.send(msg)

# Receive response
resp = sock.recv(1024)
print(f"Received {len(resp)} bytes:")
print(resp.hex())

sock.close()
```

**Expected:** Should receive an Rversion response (type 0x65)

---

## Troubleshooting Checklist

- [ ] PicoCalc shows IP address in menu (not "Joining")
- [ ] `wifi.ip()` returns valid IP (not "0.0.0.0")
- [ ] `wifi.p9_status().running` returns `true`
- [ ] Can ping PicoCalc from Linux: `ping <ip>`
- [ ] Port 564 is reachable: `nc -zv <ip> 564`
- [ ] Mount point exists: `ls -ld /mnt/picocalc`
- [ ] Running mount with sudo
- [ ] No firewall blocking port 564
- [ ] Both devices on same network/subnet
- [ ] Using correct IP address from menu

---

## Known Limitations

1. **mDNS May Not Work**
   - IGMP multicast support is conditional
   - Use direct IP address instead of hostname
   - `picocalc.local` might not resolve

2. **No Authentication**
   - Server accepts all connections
   - Only use on trusted networks

3. **Single Core Performance**
   - Server runs on Core 1
   - Should not impact Lua performance on Core 0

---

## Getting Help

If issues persist after following this guide:

1. **Collect Information:**
   ```bash
   # On Linux
   sudo dmesg | grep 9p > 9p_errors.txt
   ip addr show > network_config.txt
   
   # On PicoCalc (via REPL)
   wifi.debug_info()
   status = wifi.p9_status()
   print(status.active, status.running, status.clients)
   ```

2. **Check Documentation:**
   - [Architecture Overview](9P_SERVER_ARCHITECTURE.md)
   - [User Guide](9P_USER_GUIDE.md)
   - [Build Success Report](9P_BUILD_SUCCESS.md)

3. **Common Quick Fixes:**
   - Reboot PicoCalc (power cycle)
   - Reconnect WiFi from REPL
   - Rebuild firmware with DEBUG_OUTPUT=ON
   - Try different mount options
   - Check Linux kernel has 9P support: `lsmod | grep 9p`

---

## Next Steps After Successful Mount

Once mounted successfully:

```bash
# List files
ls -la /mnt/picocalc

# Create a test file
echo "Hello from Linux" > /mnt/picocalc/test.txt

# Read it back
cat /mnt/picocalc/test.txt

# Check from PicoCalc REPL
# (file should appear in SD card)
```

---

*Last Updated: December 18, 2025*