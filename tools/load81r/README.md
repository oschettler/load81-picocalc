# LOAD81R - Remote Shell for PicoCalc

A Python-based remote shell tool for managing files and executing Lua code on the PicoCalc device over TCP/IP.

## Features

- **File Operations**: Browse, read, write, and delete files on the SD card
- **Directory Management**: Create and navigate directories
- **File Transfer**: Upload and download files between local and remote
- **Directory Sync**: Recursively synchronize directories (rsync-like)
- **Lua REPL**: Execute Lua code interactively on the device
- **Interactive Shell**: Command-line interface with history and tab completion
- **Command Mode**: Execute single commands for scripting

## Requirements

- Python 3.6 or later
- PicoCalc device with SD card
- WiFi credentials configured in `/load81/start.lua`
- Network connectivity to the PicoCalc

## Installation

No installation required. Simply run the script directly:

```bash
cd tools/load81r
./load81r.py <picocalc-ip>
```

Or use Python directly:

```bash
python3 tools/load81r/load81r.py <picocalc-ip>
```

## Usage

### Interactive Shell Mode

Start an interactive shell session:

```bash
./load81r.py 192.168.1.100
```

This provides a command prompt where you can execute multiple commands:

```
load81r:/> ls
load81r:/> cd load81
load81r:/load81> cat nex.lua
load81r:/load81> edit myprogram.lua
load81r:/load81> repl
```

### Command Mode

Execute a single command and exit:

```bash
# List directory
./load81r.py 192.168.1.100 ls /load81

# Display file contents
./load81r.py 192.168.1.100 cat /load81/nex.lua

# Download file
./load81r.py 192.168.1.100 cp remote:/load81/nex.lua ./nex.lua

# Upload file
./load81r.py 192.168.1.100 cp ./myprogram.lua remote:/load81/myprogram.lua

# Sync directory (download)
./load81r.py 192.168.1.100 rsync /load81 ./backup

# Sync directory (upload)
./load81r.py 192.168.1.100 rsync ./myproject /load81/myproject
```

## Available Commands

### File Operations

- **`cat FILE [FILE...]`** - Display contents of one or more files
- **`cp SOURCE DEST`** - Copy files between local and remote
  - Use `remote:` prefix for remote paths
  - Examples:
    - `cp remote:/file.txt ./local.txt` (download)
    - `cp ./local.txt remote:/file.txt` (upload)

### Directory Operations

- **`ls [PATH]`** - List directory contents
- **`cd [DIRECTORY]`** - Change current directory (default: /)
- **`mkdir DIRECTORY`** - Create directory

### File Management

- **`rm PATH [PATH...]`** - Delete files or directories
- **`edit FILENAME`** - Edit remote file with local editor ($EDITOR)

### Synchronization

- **`rsync SOURCE DEST`** - Synchronize directories recursively
  - Remote paths start with `/`
  - Local paths are relative or absolute
  - Examples:
    - `rsync /load81 ./backup` (download)
    - `rsync ./myproject /load81/myproject` (upload)

### Lua Execution

- **`repl`** - Enter interactive Lua REPL
  - Type `.exit` or `.quit` to exit REPL
  - Type `.help` for REPL help

### Utility

- **`help [COMMAND]`** - Show help information
- **`exit`, `quit`** - Exit shell (interactive mode only)

## Protocol

LOAD81R uses a simple text-based protocol over TCP port 1900:

- Commands are sent as text lines
- Responses start with `+OK`, `-ERR`, or `+DATA`
- Binary data is prefixed with length
- Single-client access for SD card safety

See [`plans/load81r_architecture.md`](../../plans/load81r_architecture.md) for detailed protocol specification.

## Examples

### Basic File Management

```bash
# Connect and explore
./load81r.py 192.168.1.100
load81r:/> ls
load81r:/> cd load81
load81r:/load81> ls
load81r:/load81> cat nex.lua
```

### Backup SD Card

```bash
# Download entire SD card
./load81r.py 192.168.1.100 rsync / ./picocalc-backup
```

### Deploy Project

```bash
# Upload project directory
./load81r.py 192.168.1.100 rsync ./my-game /load81/my-game
```

### Edit Files

```bash
# Edit file with local editor
./load81r.py 192.168.1.100 edit /load81/myprogram.lua
```

### Execute Lua Code

```bash
# Start REPL
./load81r.py 192.168.1.100 repl
lua> print("Hello from PicoCalc!")
lua> x = 42
lua> print(x * 2)
lua> .exit
```

## Configuration

### WiFi Configuration

WiFi connection is configured in the Lua startup script on the SD card:

```lua
-- Edit /load81/start.lua on SD card
local WIFI_SSID = "YourNetworkName"
local WIFI_PASSWORD = "YourPassword"

if wifi.connect(WIFI_SSID, WIFI_PASSWORD) then
    print("WiFi connected!")
    print("IP Address: " .. wifi.ip())
end
```

The file server starts automatically after successful WiFi connection.

### Server Port

The server listens on port 1900 by default. To use a different port:

```bash
./load81r.py 192.168.1.100 -p 1234
```

### Editor

The `edit` command uses the `$EDITOR` environment variable. Set it to your preferred editor:

```bash
export EDITOR=nano
./load81r.py 192.168.1.100 edit /load81/myfile.lua
```

Common editors: `nano`, `vi`, `vim`, `emacs`, `code`

## Troubleshooting

### Cannot Connect

- Verify PicoCalc is powered on and WiFi is connected
- Check IP address is correct (use `nmap` or check router)
- Ensure firewall allows port 1900
- Try pinging the device: `ping 192.168.1.100`

### Connection Refused

- Server may not be running on PicoCalc
- Check firmware build includes file server
- Verify WiFi connection is established

### File Operations Fail

- Check SD card is inserted and mounted
- Verify file paths are correct (case-sensitive)
- Ensure sufficient space for uploads
- Check file permissions

### REPL Not Working

- Verify Lua interpreter is running on Core 0
- Check inter-core FIFO communication
- Try simple expressions first: `print(1+1)`

## Architecture

LOAD81R consists of two components:

1. **Server (C)** - Runs on PicoCalc Core 1
   - TCP server on port 1900
   - File system operations via FAT32
   - Lua REPL via inter-core FIFO
   - Single-client access control

2. **Client (Python)** - Runs on host computer
   - Protocol client implementation
   - Command handlers
   - Interactive shell with readline
   - File transfer and sync

See [`plans/load81r_architecture.md`](../../plans/load81r_architecture.md) for detailed architecture.

## Development

### Project Structure

```
tools/load81r/
├── load81r.py      # Main entry point
├── client.py       # Protocol client
├── commands.py     # Command implementations
├── shell.py        # Interactive shell
└── README.md       # This file
```

### Server Files

```
src/
├── picocalc_file_server.h/c    # TCP server
├── picocalc_fs_handler.h/c     # File system operations
└── picocalc_repl_handler.h/c   # Lua REPL integration
```

## License

Same as LOAD81 PicoCalc firmware (see LICENSE.md)

## See Also

- [Architecture Document](../../plans/load81r_architecture.md)
- [Implementation Plan](../../plans/load81r_implementation_plan.md)
- [PicoCalc Documentation](../../README.md)