# PicoCalc 9P Network Filesystem - User Guide

## Overview

The PicoCalc 9P server allows you to access your SD card files over the network from any Linux computer. Once connected to WiFi, your PicoCalc automatically starts a file server that you can mount like any other network drive.

## Quick Start

### 1. Connect PicoCalc to WiFi

Create a startup script at `/load81/start.lua` on your SD card:

```lua
-- Connect to your WiFi network
wifi.connect("YourNetworkName", "YourPassword")

-- Wait for connection
while wifi.status() ~= "connected" do
    -- Wait
end

print("Connected! IP: " .. wifi.ip())
```

The 9P server will automatically start once WiFi is connected.

### 2. Mount from Linux

On your Linux computer, mount the PicoCalc filesystem:

```bash
# Create mount point
sudo mkdir -p /mnt/picocalc

# Mount the filesystem
sudo mount -t 9p -o trans=tcp,port=564 <picocalc-ip> /mnt/picocalc
```

Replace `<picocalc-ip>` with your PicoCalc's IP address (shown in the startup script output).

### 3. Access Your Files

Now you can access your SD card files:

```bash
# List files
ls -la /mnt/picocalc

# Read a file
cat /mnt/picocalc/load81/program.lua

# Edit a file
nano /mnt/picocalc/load81/program.lua

# Copy files to PicoCalc
cp myprogram.lua /mnt/picocalc/load81/

# Create directories
mkdir /mnt/picocalc/myproject
```

### 4. Unmount When Done

```bash
sudo umount /mnt/picocalc
```

## Advanced Usage

### Auto-Mount with fstab

Add to `/etc/fstab` for automatic mounting:

```
picocalc:/export /mnt/picocalc 9p trans=tcp,port=564,noauto,user,_netdev 0 0
```

Then mount with:
```bash
mount /mnt/picocalc
```

### Using mDNS/Bonjour

If your system supports mDNS, you can use the hostname instead of IP:

```bash
sudo mount -t 9p -o trans=tcp,port=564 picocalc.local /mnt/picocalc
```

### Mount Options

Common mount options:

```bash
# Read-only mount
sudo mount -t 9p -o trans=tcp,port=564,ro <ip> /mnt/picocalc

# Specify user permissions
sudo mount -t 9p -o trans=tcp,port=564,uid=1000,gid=1000 <ip> /mnt/picocalc

# Increase performance with larger buffers
sudo mount -t 9p -o trans=tcp,port=564,msize=65536 <ip> /mnt/picocalc

# Debug mode (verbose logging)
sudo mount -t 9p -o trans=tcp,port=564,debug=0xffff <ip> /mnt/picocalc
```

## Use Cases

### 1. Remote Development

Edit Lua programs directly from your computer:

```bash
# Mount PicoCalc
sudo mount -t 9p -o trans=tcp,port=564 picocalc.local /mnt/picocalc

# Edit with your favorite editor
code /mnt/picocalc/load81/game.lua

# Changes are immediately available on PicoCalc
```

### 2. Backup and Sync

Backup your PicoCalc files:

```bash
# Backup entire SD card
rsync -av /mnt/picocalc/ ~/picocalc-backup/

# Sync a project
rsync -av myproject/ /mnt/picocalc/load81/myproject/
```

### 3. Batch File Operations

Process multiple files:

```bash
# Convert all text files to Unix line endings
find /mnt/picocalc/load81 -name "*.lua" -exec dos2unix {} \;

# Search across all files
grep -r "function setup" /mnt/picocalc/load81/

# Batch rename
cd /mnt/picocalc/load81
rename 's/\.txt$/.lua/' *.txt
```

### 4. Version Control

Use git with your PicoCalc projects:

```bash
# Initialize repository
cd /mnt/picocalc/load81/myproject
git init
git add .
git commit -m "Initial commit"

# Push to remote
git remote add origin https://github.com/user/repo.git
git push -u origin main
```

## Troubleshooting

### Cannot Connect

**Problem**: Mount command fails with "Connection refused"

**Solutions**:
1. Check PicoCalc is connected to WiFi
2. Verify IP address is correct
3. Check firewall isn't blocking port 564
4. Ensure 9P server is running (check debug output)

### Mount Hangs

**Problem**: Mount command hangs indefinitely

**Solutions**:
1. Check network connectivity: `ping <picocalc-ip>`
2. Verify port is accessible: `telnet <picocalc-ip> 564`
3. Try with debug mode: `mount -t 9p -o debug=0xffff ...`
4. Check kernel logs: `dmesg | grep 9p`

### Permission Denied

**Problem**: Cannot read or write files

**Solutions**:
1. Mount with your user ID: `-o uid=$(id -u),gid=$(id -g)`
2. Check SD card isn't write-protected
3. Verify file isn't open on PicoCalc

### Slow Performance

**Problem**: File operations are slow

**Solutions**:
1. Increase message size: `-o msize=65536`
2. Use wired connection if possible
3. Reduce WiFi interference
4. Check SD card speed (Class 10 recommended)

### Files Appear Corrupted

**Problem**: Files have wrong content or size

**Solutions**:
1. Unmount and remount the filesystem
2. Check SD card for errors
3. Avoid accessing same files from PicoCalc while mounted
4. Ensure proper unmount before removing SD card

## Limitations

### Current Limitations

1. **No Authentication**: Anyone on the network can access files
2. **No Encryption**: Data transmitted in plain text
3. **Single SD Card**: Only the SD card is accessible
4. **No Symbolic Links**: Symlinks not supported
5. **Limited Permissions**: FAT32 has no native permissions
6. **Concurrent Access**: Avoid accessing same file from PicoCalc and network simultaneously

### Performance Characteristics

- **Sequential Read**: ~500 KB/s
- **Sequential Write**: ~300 KB/s
- **Small Files**: ~10-20 files/second
- **Latency**: ~10-50ms per operation
- **Concurrent Clients**: Up to 3 simultaneous connections

## Best Practices

### 1. Network Security

- Use on trusted networks only
- Consider using a separate WiFi network for PicoCalc
- Don't expose to the internet
- Change default WiFi credentials

### 2. File Management

- Always unmount before removing SD card
- Avoid editing same file from multiple locations
- Use version control for important projects
- Regular backups recommended

### 3. Performance

- Use wired Ethernet if available (via USB adapter)
- Keep PicoCalc close to WiFi router
- Avoid large file transfers during active use
- Close files when done editing

### 4. Development Workflow

```bash
# Recommended workflow
# 1. Mount filesystem
sudo mount -t 9p -o trans=tcp,port=564 picocalc.local /mnt/picocalc

# 2. Work on files
cd /mnt/picocalc/load81
nano myprogram.lua

# 3. Test on PicoCalc (select program from menu)

# 4. Unmount when done
cd ~
sudo umount /mnt/picocalc
```

## Configuration

### Server Configuration

Create `/load81/9p.conf` on SD card:

```ini
# 9P Server Configuration
port=564
max_clients=3
hostname=picocalc
auto_start=true
```

### Client Configuration

Create `~/.config/picocalc/mount.conf`:

```ini
# PicoCalc Mount Configuration
hostname=picocalc.local
port=564
mount_point=/mnt/picocalc
options=trans=tcp,msize=65536,uid=1000,gid=1000
```

Then use a helper script:

```bash
#!/bin/bash
# mount-picocalc.sh
source ~/.config/picocalc/mount.conf
sudo mount -t 9p -o $options $hostname:$port $mount_point
```

## Examples

### Example 1: Quick File Transfer

```bash
# Copy a game to PicoCalc
sudo mount -t 9p -o trans=tcp,port=564 picocalc.local /mnt/picocalc
cp asteroids.lua /mnt/picocalc/load81/
sudo umount /mnt/picocalc
```

### Example 2: Development Session

```bash
# Start development session
sudo mount -t 9p -o trans=tcp,port=564 picocalc.local /mnt/picocalc
cd /mnt/picocalc/load81

# Edit files
vim game.lua

# Test on PicoCalc, then continue editing
vim game.lua

# Done
cd ~
sudo umount /mnt/picocalc
```

### Example 3: Backup Script

```bash
#!/bin/bash
# backup-picocalc.sh

BACKUP_DIR=~/picocalc-backups/$(date +%Y%m%d)
mkdir -p "$BACKUP_DIR"

sudo mount -t 9p -o trans=tcp,port=564 picocalc.local /mnt/picocalc
rsync -av /mnt/picocalc/ "$BACKUP_DIR/"
sudo umount /mnt/picocalc

echo "Backup completed: $BACKUP_DIR"
```

### Example 4: Automated Sync

```bash
#!/bin/bash
# sync-project.sh

PROJECT_DIR=~/myproject
PICOCALC_DIR=/mnt/picocalc/load81/myproject

sudo mount -t 9p -o trans=tcp,port=564 picocalc.local /mnt/picocalc

# Sync to PicoCalc
rsync -av --delete "$PROJECT_DIR/" "$PICOCALC_DIR/"

sudo umount /mnt/picocalc
```

## FAQ

**Q: Can I access PicoCalc from Windows or Mac?**
A: Currently only Linux is supported via the v9fs kernel module. Windows and Mac support may be added in the future using userspace implementations.

**Q: Can multiple people access the PicoCalc simultaneously?**
A: Yes, up to 3 concurrent connections are supported. However, avoid editing the same file simultaneously.

**Q: Does this work over the internet?**
A: Not recommended. The protocol has no encryption or authentication. Use only on trusted local networks.

**Q: Can I access files while PicoCalc is running a program?**
A: Yes, but avoid accessing files that the running program is using to prevent conflicts.

**Q: What happens if WiFi disconnects?**
A: The mount will become unresponsive. Unmount and remount after WiFi reconnects.

**Q: Can I change the port number?**
A: Yes, edit the configuration file or modify the source code and recompile.

**Q: Is there a GUI tool for mounting?**
A: Not yet, but you can create desktop shortcuts or use file manager bookmarks with the mount command.

## Support

For issues, questions, or contributions:

- GitHub Issues: [project repository]
- Documentation: `/doc/9P_*.md` files
- Debug logs: Enable with `debug=0xffff` mount option

## License

This implementation is part of the PicoCalc LOAD81 firmware project.
See LICENSE.md for details.