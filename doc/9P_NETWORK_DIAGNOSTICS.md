# 9P Server Network Diagnostics Guide

## Current Status: ✓ DEVICE IS CONNECTED

The PicoCalc is successfully connected to the network at **192.168.178.122** as confirmed by the nex.lua client being able to retrieve content from the server.

## Why Ping Fails

The PicoCalc firmware does **not respond to ICMP echo requests (ping)** by default. This is normal for embedded devices and does not indicate a connectivity problem.

**Expected behavior:**
```bash
$ ping 192.168.178.122
From 192.168.178.96 icmp_seq=1 Destination Host Unreachable
```

This is **NORMAL** - the device is connected, it just doesn't respond to ping.

## Testing 9P Server Connectivity

### 1. Check if Port 564 is Open

Use `nc` (netcat) to test if the 9P server is listening:

```bash
nc -zv 192.168.178.122 564
```

**Expected output if server is running:**
```
Connection to 192.168.178.122 564 port [tcp/*] succeeded!
```

**If connection fails:**
- Server may not be running
- Check server status in REPL (see below)

### 2. Check Server Status from REPL

In the PicoCalc REPL, use the new diagnostic function:

```lua
=wifi.p9_info()
```

This will show:
- Whether server is active
- Whether server is running
- Number of connected clients
- Port number (should be 564)

**Example output:**
```
9P: Active, Running: Yes, Clients: 0, Port: 564
```

### 3. Alternative: Check Individual Fields

If you need to check specific fields programmatically:

```lua
status = wifi.p9_status()
=status.running    -- Should be true
=status.active     -- Should be true
=status.port       -- Should be 564
=status.clients    -- Number of connected clients
```

## Mounting the 9P Filesystem

### Method 1: Direct Mount (Recommended)

```bash
sudo mount -t 9p -o trans=tcp,port=564,version=9p2000.u 192.168.178.122 /mnt/picocalc
```

### Method 2: With Additional Options

For better compatibility and debugging:

```bash
sudo mount -t 9p -o trans=tcp,port=564,version=9p2000.u,debug=1,noextend 192.168.178.122 /mnt/picocalc
```

Options explained:
- `trans=tcp` - Use TCP transport
- `port=564` - 9P server port
- `version=9p2000.u` - Protocol version
- `debug=1` - Enable debug output (optional)
- `noextend` - Disable 9P2000.u extensions (optional, for compatibility)

### Method 3: Using /etc/fstab

Add to `/etc/fstab` for automatic mounting:

```
192.168.178.122 /mnt/picocalc 9p trans=tcp,port=564,version=9p2000.u,noauto,user 0 0
```

Then mount with:
```bash
mount /mnt/picocalc
```

## Troubleshooting Mount Failures

### Error: "Protocol not supported"

**Cause:** 9P kernel module not loaded

**Solution:**
```bash
sudo modprobe 9p
sudo modprobe 9pnet
sudo modprobe 9pnet_tcp
```

Verify modules are loaded:
```bash
lsmod | grep 9p
```

### Error: "Connection refused"

**Cause:** 9P server not running on PicoCalc

**Solution:**
1. Check server status: `=wifi.p9_info()` in REPL
2. If not running, reconnect WiFi:
   ```lua
   wifi.disconnect()
   wifi.connect("YourSSID", "YourPassword")
   ```
3. Wait for connection, then check again

### Error: "No route to host"

**Cause:** Network connectivity issue (but we know device is connected)

**Solution:**
1. Verify IP address is correct: `=wifi.ip()` in REPL
2. Check if port 564 is reachable: `nc -zv 192.168.178.122 564`
3. Try mounting with explicit IP and port (see Method 1 above)

### Error: "Permission denied"

**Cause:** Need root privileges to mount

**Solution:**
```bash
sudo mount -t 9p -o trans=tcp,port=564 192.168.178.122 /mnt/picocalc
```

## Verifying Successful Mount

After mounting, verify the filesystem:

```bash
# List files
ls -la /mnt/picocalc

# Check mount point
mount | grep picocalc

# Test read access
cat /mnt/picocalc/load81/README.md

# Test write access
echo "test" > /mnt/picocalc/test.txt
cat /mnt/picocalc/test.txt
rm /mnt/picocalc/test.txt
```

## Unmounting

```bash
sudo umount /mnt/picocalc
```

## Network Connectivity Summary

✓ **Device is connected:** 192.168.178.122
✓ **nex.lua client works:** Confirmed by server logs
✗ **Ping doesn't work:** Expected - ICMP not implemented
? **9P server status:** Check with `=wifi.p9_info()` in REPL
? **Port 564 accessible:** Test with `nc -zv 192.168.178.122 564`

## Next Steps

1. **Check server status in REPL:**
   ```lua
   =wifi.p9_info()
   ```

2. **Test port connectivity:**
   ```bash
   nc -zv 192.168.178.122 564
   ```

3. **Attempt mount:**
   ```bash
   sudo mount -t 9p -o trans=tcp,port=564,version=9p2000.u 192.168.178.122 /mnt/picocalc
   ```

4. **If mount fails, check kernel logs:**
   ```bash
   dmesg | tail -20
   ```

5. **Enable debug output for detailed diagnostics:**
   ```bash
   sudo mount -t 9p -o trans=tcp,port=564,version=9p2000.u,debug=65535 192.168.178.122 /mnt/picocalc
   dmesg | tail -50