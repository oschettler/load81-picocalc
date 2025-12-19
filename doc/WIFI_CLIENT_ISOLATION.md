# WiFi Client Isolation Issue - 9P Server Not Accessible

## Problem Summary

**Symptoms:**
- ✓ PicoCalc shows IP address: 192.168.178.122
- ✓ `wifi.p9_info()` shows: "9P: Active, Running: Yes"
- ✓ nex.lua client on PicoCalc can fetch content from Linux machine
- ✗ Linux cannot ping PicoCalc (expected - ICMP not implemented)
- ✗ Linux cannot connect to PicoCalc port 564: "No route to host"

**Root Cause:** WiFi Client Isolation (AP Isolation)

## What is WiFi Client Isolation?

WiFi Client Isolation (also called AP Isolation) is a security feature on routers that prevents WiFi clients from communicating directly with each other. When enabled:

- Clients can connect to the internet ✓
- Clients can connect to the router ✓
- Clients can make outgoing connections ✓
- Clients **CANNOT** accept incoming connections from other WiFi clients ✗
- Clients **CANNOT** see each other on the network ✗

This is why:
- **nex.lua works**: PicoCalc initiates outgoing connection to Linux
- **9P server doesn't work**: Linux tries incoming connection to PicoCalc (blocked)

## Solutions

### Solution 1: Disable WiFi Client Isolation (Recommended)

**Access your router settings** (usually at http://192.168.178.1 or http://192.168.1.1):

1. Log into router admin interface
2. Navigate to WiFi/Wireless settings
3. Look for one of these options:
   - "Client Isolation" → Disable
   - "AP Isolation" → Disable  
   - "Station Isolation" → Disable
   - "Wireless Isolation" → Disable
   - "Allow clients to communicate" → Enable
4. Save settings and restart router if needed

**Common router locations:**
- **Fritz!Box**: WLAN → Security → "WLAN devices may communicate with each other"
- **TP-Link**: Wireless → Wireless Settings → "Enable AP Isolation" (uncheck)
- **Netgear**: Wireless Settings → "Enable Wireless Isolation" (uncheck)
- **Asus**: Wireless → Professional → "Set AP isolated" (No)
- **Linksys**: Wireless → Advanced → "AP Isolation" (Disabled)

### Solution 2: Use Wired Connection

Connect the Linux machine via Ethernet cable to the router. Wired connections are typically not subject to client isolation.

### Solution 3: Create Guest Network Exception

Some routers allow guest network devices to access main network:
1. Keep PicoCalc on main WiFi network
2. Configure guest network to allow access to main network
3. This varies by router model

### Solution 4: Use Different Network Topology

**Option A: Direct Connection**
- Use a separate WiFi access point without client isolation
- Connect both devices to this AP

**Option B: Bridge Mode**
- If you have a second router, configure it in bridge mode
- Connect both devices through the bridge

## Verification Steps

### 1. Check if Client Isolation is the Problem

From Linux, try to ping another WiFi device on your network:

```bash
# Try pinging your phone or another WiFi device
ping 192.168.178.XXX
```

If you can't ping ANY WiFi devices but can ping wired devices, client isolation is enabled.

### 2. Test After Disabling Isolation

After disabling client isolation:

```bash
# Test port connectivity
nc -zv 192.168.178.122 564

# If successful, try mounting
sudo mount -t 9p -o trans=tcp,port=564,version=9p2000.u 192.168.178.122 /mnt/picocalc
```

### 3. Verify 9P Server is Still Running

On PicoCalc REPL:
```lua
=wifi.p9_info()
```

Should show: "9P: Active, Running: Yes, Clients: 0, Port: 564"

## Alternative: Port Forwarding (Not Recommended)

Some routers allow port forwarding from WAN to WiFi clients, but this:
- Exposes your PicoCalc to the internet (security risk)
- Requires dynamic DNS or static IP
- Is overly complex for local file access
- **Not recommended** - disable client isolation instead

## Testing Client Isolation Status

### Quick Test Script

Save as `test_isolation.sh`:

```bash
#!/bin/bash
echo "Testing WiFi Client Isolation..."
echo ""
echo "Your IP: $(hostname -I | awk '{print $1}')"
echo "PicoCalc IP: 192.168.178.122"
echo ""

# Test ping (will fail on PicoCalc anyway, but tests routing)
echo "1. Testing ping (may fail - PicoCalc doesn't respond to ping):"
ping -c 1 -W 1 192.168.178.122 2>&1 | grep -E "(bytes from|Destination Host Unreachable|Network is unreachable)"

# Test port 564
echo ""
echo "2. Testing 9P port 564:"
nc -zv -w 2 192.168.178.122 564 2>&1

# Test if we can reach router
echo ""
echo "3. Testing router connectivity:"
ping -c 1 -W 1 192.168.178.1 2>&1 | grep "bytes from"

echo ""
if nc -zv -w 2 192.168.178.122 564 2>&1 | grep -q "succeeded"; then
    echo "✓ SUCCESS: Can connect to PicoCalc port 564"
    echo "  Client isolation is DISABLED or not present"
else
    echo "✗ FAILED: Cannot connect to PicoCalc port 564"
    echo "  This suggests WiFi client isolation is ENABLED"
    echo ""
    echo "Solution: Disable 'Client Isolation' in your router settings"
fi
```

Run with: `chmod +x test_isolation.sh && ./test_isolation.sh`

## Expected Behavior After Fix

Once client isolation is disabled:

```bash
$ nc -zv 192.168.178.122 564
Connection to 192.168.178.122 564 port [tcp/*] succeeded!

$ sudo mount -t 9p -o trans=tcp,port=564 192.168.178.122 /mnt/picocalc
$ ls /mnt/picocalc
load81/  test.txt  ...
```

## Still Not Working?

If disabling client isolation doesn't help, check:

1. **Firewall on Linux machine:**
   ```bash
   sudo ufw status
   sudo iptables -L
   ```

2. **Router firewall rules:**
   - Check if there are custom firewall rules blocking port 564
   - Some routers have separate "Internet Firewall" and "LAN Firewall"

3. **PicoCalc IP changed:**
   ```lua
   =wifi.ip()  -- Verify IP is still 192.168.178.122
   ```

4. **Server stopped:**
   ```lua
   =wifi.p9_info()  -- Should show "Running: Yes"
   ```

## Summary

**Most likely cause:** WiFi Client Isolation is enabled on your router

**Solution:** Access router settings and disable "Client Isolation" / "AP Isolation"

**Why this happens:** Security feature prevents WiFi clients from accepting incoming connections from other WiFi clients

**Why nex.lua works:** Outgoing connections (client mode) are allowed, incoming connections (server mode) are blocked