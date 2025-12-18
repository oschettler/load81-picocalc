# 9P Server Implementation Status

## Overview
This document tracks the implementation progress of the 9P2000.u protocol server for PicoCalc firmware.

**Last Updated:** 2025-12-18  
**Status:** Phase 1-2 Complete, Phase 3-6 Pending

---

## Implementation Phases

### ✅ Phase 1: Protocol Layer (COMPLETE)
**Files Created:**
- `src/picocalc_9p_proto.h` - Protocol definitions and structures
- `src/picocalc_9p_proto.c` - Message serialization/deserialization

**Implemented:**
- All 9P2000.u message type definitions (13 request/response pairs)
- QID structure with type/version/path fields
- Stat structure with Unix extensions (uid/gid/muid)
- Message buffer management for serialization
- Little-endian read/write functions for all data types
- String and QID serialization/deserialization
- Stat structure serialization with proper size calculation
- Message initialization and finalization functions
- Utility functions for debugging and validation

**Status:** ✅ Complete and ready for integration

---

### ✅ Phase 2: Thread-Safe FAT32 Layer (COMPLETE)
**Files Created:**
- `src/picocalc_fat32_sync.h` - Thread-safe FAT32 wrapper API
- `src/picocalc_fat32_sync.c` - Mutex-protected FAT32 operations

**Implemented:**
- Mutex initialization for cross-core synchronization
- Manual lock/unlock functions for batch operations
- Thread-safe wrappers for all FAT32 file operations:
  - open, create, close, read, write, seek, tell, size, eof
  - delete, rename
- Thread-safe directory operations:
  - dir_read, dir_create, set_current_dir, get_current_dir
- Thread-safe filesystem queries:
  - is_ready, get_status, get_free_space, get_total_space
  - get_volume_name, get_cluster_size
- Configurable timeout handling (5 second default)

**Status:** ✅ Complete and ready for integration

---

### ⏳ Phase 3: Core Server Infrastructure (HEADER ONLY)
**Files Created:**
- `src/picocalc_9p.h` - Server API and data structures

**Defined (Header Only):**
- Server configuration constants (port, max clients, message size)
- FID management structures and types
- Client connection state machine
- Server state enumeration
- Statistics tracking structure
- Public API declarations:
  - Server lifecycle (init, start, stop, poll)
  - Server status queries
  - FID table management
  - Message handler function signatures

**Pending Implementation:**
- `src/picocalc_9p.c` - Core server implementation
  - TCP server setup using lwIP
  - Client connection management
  - Message dispatch loop
  - FID table operations
  - Error handling and recovery

**Status:** ⏳ Header complete, implementation pending

---

### ⏳ Phase 4: Message Handlers (NOT STARTED)
**Files Needed:**
- `src/picocalc_9p_handlers.c` - All 13 message handler implementations

**Pending Implementation:**
- `p9_handle_version()` - Protocol version negotiation
- `p9_handle_attach()` - Root filesystem attachment
- `p9_handle_walk()` - Directory traversal
- `p9_handle_open()` - File/directory opening
- `p9_handle_create()` - File/directory creation
- `p9_handle_read()` - File/directory reading
- `p9_handle_write()` - File writing
- `p9_handle_clunk()` - FID release
- `p9_handle_remove()` - File/directory deletion
- `p9_handle_stat()` - Get file metadata
- `p9_handle_wstat()` - Set file metadata
- `p9_handle_auth()` - Authentication (stub)
- `p9_handle_flush()` - Cancel pending request

**Status:** ⏳ Not started

---

### ⏳ Phase 5: Filesystem Operations (NOT STARTED)
**Files Needed:**
- `src/picocalc_9p_fs.c` - FAT32 to 9P mapping layer

**Pending Implementation:**
- Path resolution and validation
- QID generation from FAT32 metadata
- Stat structure population from FAT32 entries
- Directory entry encoding for Tread responses
- Permission and attribute mapping
- Timestamp conversion (FAT32 ↔ Unix)
- Error code translation (FAT32 → 9P)

**Status:** ⏳ Not started

---

### ⏳ Phase 6: Integration & Services (NOT STARTED)
**Files Needed:**
- `src/picocalc_mdns.h` - mDNS service discovery
- `src/picocalc_mdns.c` - mDNS responder implementation
- Updates to `src/main.c` - Core 1 initialization
- Updates to `src/picocalc_wifi.c` - Auto-start callback
- Updates to `CMakeLists.txt` - Build configuration

**Pending Implementation:**
- mDNS responder for service discovery
- Core 1 initialization and 9P server launch
- WiFi connection callback integration
- CMakeLists.txt updates to include all new files
- Link against lwIP and Pico SDK libraries
- Compile-time configuration options

**Status:** ⏳ Not started

---

## File Summary

### Completed Files (4)
1. ✅ `src/picocalc_9p_proto.h` (285 lines)
2. ✅ `src/picocalc_9p_proto.c` (450 lines)
3. ✅ `src/picocalc_fat32_sync.h` (150 lines)
4. ✅ `src/picocalc_fat32_sync.c` (280 lines)

### Partial Files (1)
5. ⏳ `src/picocalc_9p.h` (280 lines) - Header only

### Pending Files (5)
6. ⏳ `src/picocalc_9p.c` - Core server (~800 lines estimated)
7. ⏳ `src/picocalc_9p_handlers.c` - Message handlers (~1200 lines estimated)
8. ⏳ `src/picocalc_9p_fs.c` - Filesystem operations (~600 lines estimated)
9. ⏳ `src/picocalc_mdns.h` - mDNS header (~100 lines estimated)
10. ⏳ `src/picocalc_mdns.c` - mDNS implementation (~400 lines estimated)

### Integration Updates Needed (3)
11. ⏳ `src/main.c` - Add Core 1 initialization
12. ⏳ `src/picocalc_wifi.c` - Add auto-start callback
13. ⏳ `CMakeLists.txt` - Add new source files and libraries

---

## Estimated Remaining Work

### Lines of Code
- **Completed:** ~1,165 lines
- **Remaining:** ~3,100 lines
- **Total Project:** ~4,265 lines

### Time Estimate
- **Phase 3 (Core Server):** 2-3 hours
- **Phase 4 (Handlers):** 3-4 hours
- **Phase 5 (Filesystem):** 2-3 hours
- **Phase 6 (Integration):** 1-2 hours
- **Testing & Debugging:** 2-4 hours
- **Total Remaining:** 10-16 hours

---

## Next Steps

### Option A: Continue Phased Implementation
Continue with Phase 3 (Core Server Infrastructure), implementing one phase at a time with testing between phases.

**Advantages:**
- Incremental progress with validation
- Easier to debug issues
- Can test components independently

**Disadvantages:**
- Slower overall progress
- More context switching

### Option B: Complete Implementation in One Session
Implement all remaining phases (3-6) in a single comprehensive session.

**Advantages:**
- Faster overall completion
- Better code consistency
- Single integration point

**Disadvantages:**
- Larger code review needed
- More complex debugging if issues arise
- Requires longer uninterrupted session

### Option C: Focus on Critical Path
Implement only the essential components needed for basic functionality, deferring advanced features.

**Critical Path:**
- Phase 3: Core server (simplified, single client)
- Phase 4: Essential handlers (version, attach, walk, open, read, clunk)
- Phase 5: Basic filesystem operations (read-only initially)
- Phase 6: Minimal integration (no mDNS, manual IP)

**Advantages:**
- Fastest path to working prototype
- Can add features incrementally
- Easier initial testing

**Disadvantages:**
- Limited functionality initially
- May need refactoring later

---

## Testing Strategy

### Unit Testing
- Protocol serialization/deserialization
- FID table operations
- Path resolution
- QID generation

### Integration Testing
- Client connection/disconnection
- Message flow (version → attach → walk → open → read)
- Multi-client scenarios
- Error handling

### System Testing
- Linux v9fs client mounting
- File operations (ls, cat, cp, rm, mkdir)
- Performance under load
- Stability over extended periods

### Test Environment
- PicoCalc hardware with WiFi connected
- Linux workstation on same network
- SD card with test filesystem
- Network monitoring tools (tcpdump, wireshark)

---

## Known Limitations

1. **Authentication:** Stub implementation only (no real security)
2. **Concurrent Clients:** Limited to 3 simultaneous connections
3. **Message Size:** 8KB maximum (negotiable down, not up)
4. **Performance:** Embedded system constraints limit throughput
5. **File Locking:** Not implemented (single-writer assumption)
6. **Extended Attributes:** Not supported by FAT32
7. **Symbolic Links:** Not supported by FAT32
8. **Hard Links:** Not supported by FAT32

---

## Dependencies

### Pico SDK Libraries
- `pico_stdlib` - Standard library
- `pico_cyw43_arch_lwip_poll` - WiFi and lwIP networking
- `pico_sync` - Mutex and synchronization primitives
- `pico_multicore` - Dual-core support
- `hardware_spi` - SD card communication
- `hardware_gpio` - GPIO control

### External Libraries
- lwIP - Lightweight TCP/IP stack
- FatFs or custom FAT32 driver (already in project)

### Build Tools
- CMake 3.13+
- ARM GCC toolchain
- Pico SDK 1.5.0+

---

## Documentation

### Completed
- ✅ Architecture design (`9P_SERVER_ARCHITECTURE.md`)
- ✅ Implementation guide (`9P_IMPLEMENTATION_GUIDE.md`)
- ✅ User guide (`9P_USER_GUIDE.md`)
- ✅ Project summary (`9P_PROJECT_SUMMARY.md`)
- ✅ Implementation status (this document)

### Pending
- ⏳ API reference documentation
- ⏳ Troubleshooting guide
- ⏳ Performance tuning guide
- ⏳ Security considerations

---

## Questions for User

1. **Which implementation approach do you prefer?**
   - A: Phased (continue with Phase 3)
   - B: Complete (all phases at once)
   - C: Critical path (minimal viable product)

2. **Do you want to review code as we go, or at the end?**
   - Incremental review after each phase
   - Final review after completion

3. **Are there any specific features or constraints we should prioritize?**
   - Performance optimization
   - Memory efficiency
   - Feature completeness
   - Code simplicity

4. **Do you have a preference for testing strategy?**
   - Test each phase independently
   - Integration testing after completion
   - Both unit and integration tests

---

## Contact & Support

For questions or issues during implementation:
- Review architecture documentation in `doc/9P_SERVER_ARCHITECTURE.md`
- Check implementation guide in `doc/9P_IMPLEMENTATION_GUIDE.md`
- Refer to 9P2000.u protocol specification
- Consult Pico SDK documentation for platform-specific details