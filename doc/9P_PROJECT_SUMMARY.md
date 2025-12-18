# 9P2000.u Server Implementation - Project Summary

## Executive Summary

This document provides a comprehensive architectural plan for implementing a 9P2000.u protocol server on the PicoCalc firmware, enabling Linux systems to mount the SD card filesystem over TCP/IP using the standard v9fs client.

## Project Goals

### Primary Objectives
1. ✅ Enable network access to PicoCalc SD card from Linux systems
2. ✅ Implement full 9P2000.u protocol with Unix extensions
3. ✅ Run server on Core 1 with zero impact on Core 0 (Lua/UI)
4. ✅ Support 2-3 concurrent client connections
5. ✅ Provide thread-safe SD card access with mutex protection
6. ✅ Auto-start server after WiFi connection
7. ✅ Implement mDNS service discovery (picocalc.local)

### Technical Requirements
- **Protocol**: 9P2000.u (Unix extensions)
- **Transport**: TCP/IP over WiFi
- **Port**: 564 (standard 9P port)
- **Authentication**: None (network security model)
- **Concurrency**: 3 simultaneous clients
- **Memory Budget**: ~64KB on Core 1
- **Performance**: >500 KB/s sequential reads

## Architecture Overview

### Dual-Core Design

```
Core 0 (Main)              Core 1 (9P Server)
├─ Lua Interpreter         ├─ TCP Server (lwIP)
├─ Graphics Engine         ├─ 9P Protocol Handler
├─ UI/Menu System          ├─ Client Management
├─ WiFi Manager            ├─ mDNS Responder
└─ Keyboard Input          └─ Message Processing
         │                          │
         └──────────┬───────────────┘
                    │
              ┌─────▼─────┐
              │   Mutex   │
              │ Protected │
              │ FAT32 API │
              └─────┬─────┘
                    │
              ┌─────▼─────┐
              │  SD Card  │
              │   (SPI)   │
              └───────────┘
```

### Key Components

1. **Protocol Layer** (`picocalc_9p_proto.c`)
   - Message parsing and serialization
   - 9P2000.u data types and structures
   - Protocol state machine

2. **Server Core** (`picocalc_9p.c`)
   - TCP server using lwIP
   - Client connection management
   - FID (File ID) tracking
   - Buffer pool management

3. **Filesystem Layer** (`picocalc_9p_fs.c`)
   - FAT32 to 9P mapping
   - QID generation
   - Directory operations
   - File operations

4. **Synchronization** (`picocalc_fat32_sync.c`)
   - Mutex-protected FAT32 wrappers
   - Thread-safe SD card access
   - Deadlock prevention

5. **Service Discovery** (`picocalc_mdns.c`)
   - mDNS responder
   - Service advertisement
   - Hostname resolution (picocalc.local)

## Implementation Status

### Completed Design Work
- ✅ Full protocol specification documented
- ✅ System architecture designed
- ✅ Component interfaces defined
- ✅ Memory layout planned
- ✅ Synchronization strategy defined
- ✅ Error handling approach documented
- ✅ Testing strategy outlined
- ✅ User documentation written

### Ready for Implementation
All architectural decisions have been made. The implementation can proceed in phases:

**Phase 1**: Protocol foundation (message parsing)
**Phase 2**: Thread-safe FAT32 layer
**Phase 3**: Core server infrastructure
**Phase 4**: Filesystem operations
**Phase 5**: mDNS service discovery
**Phase 6**: Integration and testing

## File Structure

### New Files to Create
```
src/
├── picocalc_9p.h              # Main server header
├── picocalc_9p.c              # Server implementation
├── picocalc_9p_proto.h        # Protocol definitions
├── picocalc_9p_proto.c        # Protocol message handling
├── picocalc_9p_fs.c           # Filesystem operations
├── picocalc_fat32_sync.h      # Thread-safe FAT32 wrapper
├── picocalc_fat32_sync.c      # Mutex-protected operations
├── picocalc_mdns.h            # mDNS responder header
└── picocalc_mdns.c            # mDNS implementation

doc/
├── 9P_SERVER_ARCHITECTURE.md  # Detailed architecture
├── 9P_IMPLEMENTATION_GUIDE.md # Implementation guide
├── 9P_USER_GUIDE.md           # End-user documentation
└── 9P_PROJECT_SUMMARY.md      # This file
```

### Modified Files
```
CMakeLists.txt                 # Add new sources, link multicore
src/main.c                     # Launch Core 1 with 9P server
src/picocalc_wifi.c            # Add connection callback
src/lwipopts.h                 # Adjust memory if needed
```

## Memory Budget

### Core 1 Memory Allocation
```
Component                Size      Notes
─────────────────────────────────────────────────
Message Buffers         48 KB     6 × 8KB buffers
Client State            6 KB      3 clients × 2KB
FID Tables              6 KB      3 × 32 FIDs
Stack                   2 KB      Core 1 stack
Misc/Overhead           2 KB      Structures, etc.
─────────────────────────────────────────────────
Total                   ~64 KB    Well within limits
```

### lwIP Configuration
Current settings in `lwipopts.h` are adequate:
- `MEM_SIZE`: 16KB (sufficient)
- `MEMP_NUM_TCP_PCB`: 4 (allows 3 clients + 1 listener)
- `TCP_MSS`: 1460 (standard)
- `TCP_WND`: 11680 (8 × MSS)

## Performance Characteristics

### Expected Performance
- **Sequential Read**: 500-800 KB/s
- **Sequential Write**: 300-500 KB/s
- **Small File Operations**: 10-20 ops/sec
- **Latency**: 10-50ms per operation
- **Concurrent Clients**: 3 simultaneous

### Bottlenecks
1. SD card SPI speed (hardware limited)
2. FAT32 cluster chain traversal
3. Network packet processing
4. Mutex contention (minimal with good design)

## Security Model

### Current Approach
- **No Authentication**: Relies on network isolation
- **No Encryption**: Plain TCP (suitable for local networks)
- **Full Access**: Read/write to entire SD card
- **Trust Model**: Trusted local network only

### Recommendations
- Use on private WiFi networks only
- Do not expose to internet
- Consider WPA2-Enterprise for additional security
- Future: Add optional authentication layer

## Testing Strategy

### Unit Tests
- Protocol message encoding/decoding
- QID generation and tracking
- FID table management
- Buffer pool allocation
- Mutex protection

### Integration Tests
- Single client operations
- Multiple concurrent clients
- Large file transfers
- Directory traversal
- Error conditions

### System Tests
- Mount from various Linux distributions
- File operations (create, read, write, delete)
- Directory operations (mkdir, rmdir, ls)
- Concurrent access scenarios
- SD card removal/insertion
- WiFi disconnect/reconnect

## Usage Example

### From Linux Client
```bash
# Mount PicoCalc filesystem
sudo mount -t 9p -o trans=tcp,port=564 picocalc.local /mnt/picocalc

# Access files
ls -la /mnt/picocalc/load81/
cat /mnt/picocalc/load81/game.lua
echo "print('Hello')" > /mnt/picocalc/load81/test.lua

# Unmount
sudo umount /mnt/picocalc
```

### From PicoCalc
```lua
-- In /load81/start.lua
wifi.connect("MyNetwork", "password")
while wifi.status() ~= "connected" do end
print("9P Server: " .. wifi.ip() .. ":564")
-- Server auto-starts on Core 1
```

## Benefits

### For Users
1. **Easy File Management**: Edit files from computer
2. **No SD Card Removal**: Access files over network
3. **Version Control**: Use git with PicoCalc projects
4. **Backup**: Easy automated backups
5. **Development**: Edit with favorite IDE

### For Developers
1. **Standard Protocol**: Uses Linux v9fs client
2. **No Custom Software**: Works with standard tools
3. **Network Transparent**: Appears as local filesystem
4. **Concurrent Access**: Multiple users/tools simultaneously
5. **Extensible**: Easy to add features

## Limitations

### Current Limitations
1. Linux only (v9fs kernel module required)
2. No authentication or encryption
3. FAT32 limitations (no permissions, 4GB file limit)
4. Performance limited by SD card and WiFi
5. No symbolic link support

### Future Enhancements
1. 9P2000.L support (Linux-specific extensions)
2. Optional authentication
3. TLS encryption
4. Windows/Mac support (userspace implementations)
5. Performance optimizations (caching, batching)
6. Configuration UI in PicoCalc menu

## Risk Assessment

### Low Risk
- ✅ Protocol well-documented and stable
- ✅ lwIP proven on RP2350
- ✅ FAT32 driver already working
- ✅ Dual-core architecture well-supported

### Medium Risk
- ⚠️ Mutex contention (mitigated by design)
- ⚠️ Memory constraints (64KB budget adequate)
- ⚠️ Network reliability (handle disconnects)

### Mitigation Strategies
- Comprehensive testing at each phase
- Graceful error handling throughout
- Timeout mechanisms for all operations
- Clear debug logging for troubleshooting

## Success Criteria

### Minimum Viable Product
- [ ] Mount from Linux successfully
- [ ] Read files from SD card
- [ ] Write files to SD card
- [ ] List directory contents
- [ ] Create and delete files
- [ ] Stable with single client

### Full Feature Set
- [ ] Support 3 concurrent clients
- [ ] mDNS service discovery working
- [ ] Auto-start on WiFi connection
- [ ] Handle SD card removal gracefully
- [ ] Performance meets targets (>500 KB/s)
- [ ] Zero impact on Core 0 operations

### Production Ready
- [ ] Comprehensive error handling
- [ ] Extensive testing completed
- [ ] Documentation complete
- [ ] User feedback incorporated
- [ ] Known issues documented

## Timeline Estimate

### Phase-by-Phase Estimate
```
Phase 1: Protocol Foundation      2-3 days
Phase 2: FAT32 Synchronization    1-2 days
Phase 3: Server Infrastructure    2-3 days
Phase 4: Filesystem Operations    3-4 days
Phase 5: mDNS Discovery          1-2 days
Phase 6: Integration & Testing    3-4 days
─────────────────────────────────────────
Total Development Time:          12-18 days
```

### Milestones
1. **Week 1**: Protocol and sync layer complete
2. **Week 2**: Server and filesystem working
3. **Week 3**: Integration, testing, polish

## Conclusion

This project provides a comprehensive plan for implementing a production-quality 9P2000.u server on the PicoCalc firmware. The architecture is sound, the design is complete, and all technical challenges have been addressed.

The implementation is ready to proceed, with clear phases, well-defined interfaces, and comprehensive documentation for both developers and end users.

## Next Steps

To proceed with implementation:

1. **Review** this architectural plan
2. **Approve** the design approach
3. **Switch to Code mode** to begin implementation
4. **Start with Phase 1**: Protocol foundation
5. **Iterate** through phases with testing at each step

## References

### Documentation
- `9P_SERVER_ARCHITECTURE.md` - Detailed technical architecture
- `9P_IMPLEMENTATION_GUIDE.md` - Step-by-step implementation guide
- `9P_USER_GUIDE.md` - End-user documentation

### External Resources
- [9P2000 Protocol Specification](http://man.cat-v.org/plan_9/5/intro)
- [9P2000.u Extensions](http://ericvh.github.io/9p-rfc/rfc9p2000.u.html)
- [Linux v9fs Documentation](https://www.kernel.org/doc/Documentation/filesystems/9p.txt)
- [RP2350 Datasheet](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf)
- [lwIP Documentation](https://www.nongnu.org/lwip/)

---

**Status**: Architecture Complete - Ready for Implementation
**Last Updated**: 2025-12-18
**Architect**: Claude (Sonnet 4.5)