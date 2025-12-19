# LOAD81R Implementation Plan

## Overview

This document provides a detailed, step-by-step implementation plan for the `load81r` remote shell system. The implementation is divided into phases to ensure systematic development and testing.

## Phase 1: Server Foundation (C Implementation)

### Step 1.1: File Server Core Structure

**Files to Create:**
- `src/picocalc_file_server.h`
- `src/picocalc_file_server.c`

**Implementation Tasks:**

1. **Define data structures:**
```c
typedef struct {
    struct tcp_pcb *pcb;
    bool active;
    char rx_buffer[1024];
    uint16_t rx_len;
    char current_dir[256];
    bool authenticated;
} file_client_t;

typedef struct {
    struct tcp_pcb *listen_pcb;
    file_client_t client;
    bool running;
    uint32_t total_requests;
} file_server_t;
```

2. **Implement initialization:**
```c
bool file_server_init(void);
bool file_server_start(void);
void file_server_stop(void);
bool file_server_is_running(void);
```

3. **Implement TCP callbacks:**
```c
static err_t file_accept(void *arg, struct tcp_pcb *newpcb, err_t err);
static err_t file_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static void file_err(void *arg, err_t err);
static void file_close_client(file_client_t *client);
```

4. **Implement command parser:**
```c
static void parse_command(file_client_t *client, const char *line);
static void send_response(file_client_t *client, const char *response);
static void send_error(file_client_t *client, const char *message);
static void send_data(file_client_t *client, const uint8_t *data, size_t len);
```

**Testing:**
- Verify server starts and listens on port 1900
- Test connection with `nc localhost 1900`
- Test basic command parsing with `HELLO` command

### Step 1.2: Protocol Implementation

**Implementation Tasks:**

1. **Command handlers (stubs):**
```c
static void cmd_hello(file_client_t *client, const char *args);
static void cmd_pwd(file_client_t *client, const char *args);
static void cmd_cd(file_client_t *client, const char *args);
static void cmd_ls(file_client_t *client, const char *args);
static void cmd_cat(file_client_t *client, const char *args);
static void cmd_put(file_client_t *client, const char *args);
static void cmd_mkdir(file_client_t *client, const char *args);
static void cmd_rm(file_client_t *client, const char *args);
static void cmd_stat(file_client_t *client, const char *args);
static void cmd_repl(file_client_t *client, const char *args);
static void cmd_ping(file_client_t *client, const char *args);
static void cmd_quit(file_client_t *client, const char *args);
```

2. **Command dispatch table:**
```c
typedef void (*cmd_handler_t)(file_client_t *client, const char *args);

typedef struct {
    const char *name;
    cmd_handler_t handler;
} command_entry_t;

static const command_entry_t commands[] = {
    {"HELLO", cmd_hello},
    {"PWD", cmd_pwd},
    {"CD", cmd_cd},
    // ... etc
};
```

**Testing:**
- Test each command with stub responses
- Verify protocol format compliance
- Test error handling

### Step 1.3: File System Handler

**Files to Create:**
- `src/picocalc_fs_handler.h`
- `src/picocalc_fs_handler.c`

**Implementation Tasks:**

1. **Define API:**
```c
typedef enum {
    FS_OK = 0,
    FS_ERR_NOT_FOUND,
    FS_ERR_NOT_DIR,
    FS_ERR_NOT_FILE,
    FS_ERR_EXISTS,
    FS_ERR_NO_SPACE,
    FS_ERR_IO,
    FS_ERR_INVALID_PATH
} fs_error_t;

typedef struct {
    char name[256];
    uint32_t size;
    bool is_dir;
    uint16_t date;
    uint16_t time;
} fs_entry_t;

fs_error_t fs_normalize_path(const char *path, const char *cwd, char *out, size_t out_len);
fs_error_t fs_list_dir(const char *path, fs_entry_t **entries, size_t *count);
fs_error_t fs_read_file(const char *path, uint8_t **data, size_t *size);
fs_error_t fs_write_file(const char *path, const uint8_t *data, size_t size);
fs_error_t fs_delete(const char *path);
fs_error_t fs_mkdir(const char *path);
fs_error_t fs_stat(const char *path, fs_entry_t *entry);
const char *fs_error_string(fs_error_t error);
```

2. **Implement path operations:**
```c
// Normalize paths: handle ., .., absolute/relative
// Validate paths: no .. escaping root, valid characters
// Join paths: combine cwd with relative path
```

3. **Implement FAT32 wrappers:**
```c
// Wrap fat32_open, fat32_read, fat32_write, etc.
// Add error translation from fat32_error_t to fs_error_t
// Add JSON formatting for directory listings
```

4. **Implement JSON formatting:**
```c
static char *format_dir_json(fs_entry_t *entries, size_t count);
static char *format_stat_json(fs_entry_t *entry);
```

**Testing:**
- Test path normalization with various inputs
- Test directory listing
- Test file read/write
- Test error conditions

### Step 1.4: REPL Handler

**Files to Create:**
- `src/picocalc_repl_handler.h`
- `src/picocalc_repl_handler.c`

**Implementation Tasks:**

1. **Define inter-core communication:**
```c
typedef struct {
    char code[512];
    char output[1024];
    bool complete;
    bool error;
} repl_request_t;

// Use multicore FIFO for communication
repl_error_t repl_execute(const char *code, char **output);
```

2. **Implement Core 1 side:**
```c
// Send code to Core 0 via FIFO
// Wait for response with timeout
// Return output or error
```

3. **Implement Core 0 side:**
```c
// Poll FIFO in main loop
// Execute Lua code when request arrives
// Capture output (redirect stdout)
// Send response back via FIFO
```

**Testing:**
- Test simple Lua expressions
- Test multi-line code
- Test error handling
- Test timeout behavior

### Step 1.5: Integration

**Implementation Tasks:**

1. **Replace diagnostic server:**
   - Modify `src/picocalc_wifi.c` to start file server instead
   - Update `src/main.c` if needed
   - Remove or disable old diagnostic server

2. **Update CMakeLists.txt:**
```cmake
target_sources(picocalc PRIVATE
    src/picocalc_file_server.c
    src/picocalc_fs_handler.c
    src/picocalc_repl_handler.c
)
```

3. **Add Lua integration:**
   - Modify Lua main loop to poll REPL FIFO
   - Add output capture mechanism
   - Test with existing REPL code

**Testing:**
- Build and flash firmware
- Test WiFi connection
- Test server auto-start
- Test basic commands via netcat

## Phase 2: Python Client Foundation

### Step 2.1: Project Structure

**Create directory structure:**
```
tools/load81r/
├── load81r.py          # Main entry point
├── client.py           # Protocol client
├── shell.py            # Interactive shell
├── commands.py         # Command implementations
├── sync.py             # Rsync functionality
├── editor.py           # Editor integration
├── README.md           # User documentation
└── requirements.txt    # Python dependencies
```

**Dependencies:**
```
# requirements.txt
# No external dependencies for basic functionality
# Optional: colorama for colored output
# Optional: pygments for syntax highlighting
```

### Step 2.2: Protocol Client

**File:** `tools/load81r/client.py`

**Implementation Tasks:**

1. **Define response types:**
```python
from dataclasses import dataclass
from typing import Optional

@dataclass
class Response:
    success: bool
    data: Optional[str] = None
    error: Optional[str] = None
    binary: Optional[bytes] = None
```

2. **Implement client class:**
```python
class Load81Client:
    def __init__(self):
        self.sock = None
        self.host = None
        self.port = 1900
        
    def connect(self, host: str, port: int = 1900) -> bool:
        # Create socket, connect, send HELLO
        
    def send_command(self, cmd: str, *args) -> Response:
        # Format command, send, receive response
        
    def receive_response(self) -> Response:
        # Read response line, parse +OK/-ERR/+DATA
        
    def receive_data(self) -> bytes:
        # Read binary data after +DATA response
        
    def send_data(self, data: bytes) -> bool:
        # Send binary data after +READY response
        
    def close(self):
        # Send QUIT, close socket
```

**Testing:**
- Test connection to server
- Test command sending
- Test response parsing
- Test binary data transfer

### Step 2.3: Command Implementations

**File:** `tools/load81r/commands.py`

**Implementation Tasks:**

1. **Implement each command:**
```python
def cmd_cat(client: Load81Client, *files) -> int:
    # For each file: send CAT, receive data, print
    
def cmd_cd(client: Load81Client, path: str) -> int:
    # Send CD, check response
    
def cmd_cp(client: Load81Client, src: str, dst: str) -> int:
    # Determine direction, transfer file
    
def cmd_edit(client: Load81Client, file: str) -> int:
    # Download, edit, upload
    
def cmd_help(client: Load81Client, cmd: Optional[str] = None) -> int:
    # Display help text
    
def cmd_ls(client: Load81Client, path: Optional[str] = None) -> int:
    # Send LS, parse JSON, format output
    
def cmd_mkdir(client: Load81Client, path: str) -> int:
    # Send MKDIR, check response
    
def cmd_repl(client: Load81Client) -> int:
    # Enter REPL loop
    
def cmd_rm(client: Load81Client, *paths) -> int:
    # For each path: send RM, check response
    
def cmd_rsync(client: Load81Client, src: str, dst: str) -> int:
    # Synchronize directories
```

**Testing:**
- Test each command individually
- Test error handling
- Test edge cases

### Step 2.4: Interactive Shell

**File:** `tools/load81r/shell.py`

**Implementation Tasks:**

1. **Implement shell class:**
```python
class InteractiveShell:
    def __init__(self, client: Load81Client):
        self.client = client
        self.cwd = "/"
        self.history = []
        
    def run(self):
        # Main shell loop
        
    def get_prompt(self) -> str:
        # Return formatted prompt
        
    def parse_line(self, line: str) -> tuple:
        # Parse command and arguments
        
    def execute(self, cmd: str, args: list) -> int:
        # Dispatch to command handler
        
    def complete(self, text: str, state: int) -> Optional[str]:
        # Tab completion
```

2. **Add readline support:**
```python
import readline

# Configure history file
# Configure tab completion
# Configure key bindings
```

**Testing:**
- Test interactive mode
- Test command history
- Test tab completion
- Test prompt display

### Step 2.5: Main Entry Point

**File:** `tools/load81r/load81r.py`

**Implementation Tasks:**

1. **Implement argument parsing:**
```python
import argparse

parser = argparse.ArgumentParser(description='LOAD81 Remote Shell')
parser.add_argument('host', help='PicoCalc IP address')
parser.add_argument('command', nargs='*', help='Command to execute')
parser.add_argument('-p', '--port', type=int, default=1900)
```

2. **Implement mode selection:**
```python
def main():
    args = parser.parse_args()
    
    client = Load81Client()
    if not client.connect(args.host, args.port):
        print("Connection failed")
        return 1
    
    if args.command:
        # Command-line mode
        return execute_command(client, args.command)
    else:
        # Interactive mode
        shell = InteractiveShell(client)
        return shell.run()
```

**Testing:**
- Test command-line mode
- Test interactive mode
- Test connection errors
- Test argument parsing

## Phase 3: Advanced Features

### Step 3.1: Rsync Implementation

**File:** `tools/load81r/sync.py`

**Implementation Tasks:**

1. **Implement directory tree comparison:**
```python
def build_remote_tree(client: Load81Client, path: str) -> dict:
    # Recursively list remote directory
    
def build_local_tree(path: str) -> dict:
    # Recursively list local directory
    
def compare_trees(remote: dict, local: dict) -> dict:
    # Return: new, modified, deleted files
```

2. **Implement synchronization:**
```python
def sync_download(client: Load81Client, src: str, dst: str, progress=True):
    # Download files from remote to local
    
def sync_upload(client: Load81Client, src: str, dst: str, progress=True):
    # Upload files from local to remote
```

3. **Add progress reporting:**
```python
def show_progress(current: int, total: int, filename: str):
    # Display progress bar
```

**Testing:**
- Test download sync
- Test upload sync
- Test incremental sync
- Test progress display

### Step 3.2: Editor Integration

**File:** `tools/load81r/editor.py`

**Implementation Tasks:**

1. **Implement editor launcher:**
```python
import os
import subprocess
import tempfile

def edit_file(client: Load81Client, remote_path: str) -> bool:
    # Download file
    # Create temp file
    # Launch editor
    # Check if modified
    # Upload if changed
    # Clean up
```

2. **Add editor detection:**
```python
def find_editor() -> Optional[str]:
    # Check $EDITOR
    # Check for nano
    # Check for vi
    # Return None if not found
```

**Testing:**
- Test with different editors
- Test file modification detection
- Test upload on save
- Test cleanup

### Step 3.3: Enhanced REPL

**Implementation Tasks:**

1. **Add multi-line support:**
```python
def is_complete_statement(code: str) -> bool:
    # Check if Lua statement is complete
    # Handle incomplete blocks (if, for, function, etc.)
```

2. **Add syntax highlighting (optional):**
```python
try:
    from pygments import highlight
    from pygments.lexers import LuaLexer
    from pygments.formatters import TerminalFormatter
    SYNTAX_HIGHLIGHTING = True
except ImportError:
    SYNTAX_HIGHLIGHTING = False
```

3. **Add special commands:**
```python
REPL_COMMANDS = {
    '.exit': 'Exit REPL',
    '.help': 'Show help',
    '.clear': 'Clear screen',
}
```

**Testing:**
- Test multi-line input
- Test syntax highlighting
- Test special commands
- Test error handling

## Phase 4: Testing and Documentation

### Step 4.1: Integration Testing

**Test Scenarios:**

1. **Basic Operations:**
   - Connect to server
   - List directories
   - Read files
   - Write files
   - Delete files
   - Create directories

2. **Large File Transfer:**
   - Upload 1MB file
   - Download 1MB file
   - Verify integrity

3. **Error Conditions:**
   - File not found
   - Permission denied
   - Disk full
   - Connection lost
   - Invalid paths

4. **Concurrent Operations:**
   - Multiple commands in sequence
   - Connection timeout
   - Server restart

5. **REPL Testing:**
   - Simple expressions
   - Multi-line code
   - Error handling
   - Long-running code

### Step 4.2: Documentation

**Documents to Create:**

1. **User Guide** (`tools/load81r/README.md`):
   - Installation instructions
   - Usage examples
   - Command reference
   - Troubleshooting

2. **Developer Guide** (`plans/load81r_developer.md`):
   - Architecture overview
   - Protocol specification
   - Adding new commands
   - Debugging tips

3. **API Documentation**:
   - Server API (Doxygen comments)
   - Client API (Python docstrings)

### Step 4.3: Performance Optimization

**Optimization Tasks:**

1. **Server optimizations:**
   - Buffer size tuning
   - Memory allocation optimization
   - Response caching
   - Connection pooling (if multi-client)

2. **Client optimizations:**
   - Pipelining commands
   - Compression (optional)
   - Caching directory listings
   - Parallel transfers (if supported)

3. **Protocol optimizations:**
   - Binary protocol option
   - Chunked transfers
   - Delta sync for rsync

## Phase 5: Deployment

### Step 5.1: Build and Package

**Tasks:**

1. **Firmware build:**
   - Update build version
   - Build release firmware
   - Test on hardware
   - Create UF2 file

2. **Python package:**
   - Create setup.py
   - Add entry point
   - Test installation
   - Create distribution

### Step 5.2: Installation Script

**Create:** `tools/install_load81r.sh`

```bash
#!/bin/bash
# Install load81r tool

# Check Python version
# Install dependencies
# Copy files to /usr/local/bin
# Set permissions
# Test installation
```

### Step 5.3: User Documentation

**Create:** `doc/LOAD81R_GUIDE.md`

**Contents:**
- Quick start guide
- Installation instructions
- Command reference
- Examples
- Troubleshooting
- FAQ

## Implementation Timeline

### Week 1: Server Foundation
- Days 1-2: File server core and protocol
- Days 3-4: File system handler
- Day 5: REPL handler
- Days 6-7: Integration and testing

### Week 2: Python Client
- Days 1-2: Protocol client and basic commands
- Days 3-4: Interactive shell
- Day 5: Advanced commands (cp, rsync)
- Days 6-7: Testing and bug fixes

### Week 3: Polish and Documentation
- Days 1-2: Editor integration and REPL enhancements
- Days 3-4: Integration testing
- Days 5-6: Documentation
- Day 7: Final testing and release

## Success Criteria

1. **Functionality:**
   - All commands work as specified
   - File transfers are reliable
   - REPL is responsive
   - Error handling is robust

2. **Performance:**
   - File transfer: >100 KB/s
   - Command latency: <100ms
   - REPL response: <500ms
   - Memory usage: <64KB on server

3. **Usability:**
   - Intuitive command syntax
   - Clear error messages
   - Good documentation
   - Easy installation

4. **Reliability:**
   - No crashes or hangs
   - Graceful error recovery
   - Connection stability
   - Data integrity

## Risk Mitigation

### Technical Risks

1. **Memory constraints on RP2350:**
   - Mitigation: Use streaming for large files
   - Mitigation: Limit buffer sizes
   - Mitigation: Dynamic allocation with limits

2. **SD card access conflicts:**
   - Mitigation: Single-client enforcement
   - Mitigation: Mutex protection
   - Mitigation: Clear error messages

3. **Network reliability:**
   - Mitigation: Timeout handling
   - Mitigation: Retry logic
   - Mitigation: Connection state tracking

4. **REPL integration complexity:**
   - Mitigation: Simple FIFO-based communication
   - Mitigation: Timeout protection
   - Mitigation: Fallback to error message

### Project Risks

1. **Scope creep:**
   - Mitigation: Stick to defined features
   - Mitigation: Phase-based implementation
   - Mitigation: MVP first, enhancements later

2. **Testing challenges:**
   - Mitigation: Automated test scripts
   - Mitigation: Hardware test setup
   - Mitigation: Comprehensive test plan

3. **Documentation lag:**
   - Mitigation: Document as you code
   - Mitigation: Code comments
   - Mitigation: Dedicated documentation phase

## Next Steps

1. Review and approve this implementation plan
2. Set up development environment
3. Begin Phase 1: Server Foundation
4. Regular progress reviews
5. Adjust plan as needed based on findings

## Conclusion

This implementation plan provides a structured approach to building the `load81r` remote shell system. By following this plan phase-by-phase, we can ensure systematic development, thorough testing, and successful deployment of a robust and user-friendly tool for PicoCalc file system access and Lua REPL functionality.