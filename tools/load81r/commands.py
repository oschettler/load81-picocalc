#!/usr/bin/env python3
"""
LOAD81R Command Implementations
Handlers for all user commands
"""

import os
import sys
import tempfile
import subprocess
from typing import Optional
from client import Load81Client


def cmd_cat(client: Load81Client, *files) -> int:
    """Display file contents"""
    if not files:
        print("Error: Missing filename", file=sys.stderr)
        print("Usage: cat FILE [FILE...]", file=sys.stderr)
        return 1
    
    exit_code = 0
    for filename in files:
        data = client.cat(filename)
        if data is None:
            error_msg = client.last_error if client.last_error else "Cannot read"
            print(f"Error: {error_msg} '{filename}'", file=sys.stderr)
            exit_code = 1
            continue
        
        try:
            # Try to decode as text
            text = data.decode('utf-8')
            print(text, end='')
        except UnicodeDecodeError:
            # Binary file - write to stdout as-is
            sys.stdout.buffer.write(data)
    
    return exit_code


def cmd_cd(client: Load81Client, path: Optional[str] = None) -> int:
    """Change directory"""
    if path is None:
        path = "/"
    
    if client.cd(path):
        return 0
    else:
        print(f"Error: Cannot change to directory '{path}'", file=sys.stderr)
        return 1


def cmd_cp(client: Load81Client, src: str, dst: str) -> int:
    """Copy files between local and remote"""
    if not src or not dst:
        print("Error: Missing source or destination", file=sys.stderr)
        print("Usage: cp SOURCE DEST", file=sys.stderr)
        print("  cp remote:/path local/path  (download)", file=sys.stderr)
        print("  cp local/path remote:/path  (upload)", file=sys.stderr)
        return 1
    
    # Determine direction
    src_is_remote = src.startswith("remote:")
    dst_is_remote = dst.startswith("remote:")
    
    if src_is_remote and dst_is_remote:
        print("Error: Server-side copy not yet implemented", file=sys.stderr)
        return 1
    
    if src_is_remote:
        # Download
        remote_path = src[7:]  # Remove "remote:" prefix
        local_path = dst
        
        print(f"Downloading {remote_path} -> {local_path}")
        data = client.cat(remote_path)
        if data is None:
            print(f"Error: Cannot read remote file '{remote_path}'", file=sys.stderr)
            return 1
        
        try:
            with open(local_path, 'wb') as f:
                f.write(data)
            print(f"Downloaded {len(data)} bytes")
            return 0
        except IOError as e:
            print(f"Error: Cannot write local file: {e}", file=sys.stderr)
            return 1
    
    elif dst_is_remote:
        # Upload
        local_path = src
        remote_path = dst[7:]  # Remove "remote:" prefix
        
        try:
            with open(local_path, 'rb') as f:
                data = f.read()
        except IOError as e:
            print(f"Error: Cannot read local file: {e}", file=sys.stderr)
            return 1
        
        print(f"Uploading {local_path} -> {remote_path}")
        if client.put(remote_path, data):
            print(f"Uploaded {len(data)} bytes")
            return 0
        else:
            print(f"Error: Cannot write remote file '{remote_path}'", file=sys.stderr)
            return 1
    
    else:
        print("Error: At least one path must be prefixed with 'remote:'", file=sys.stderr)
        return 1


def cmd_edit(client: Load81Client, filename: str) -> int:
    """Edit remote file with local editor"""
    if not filename:
        print("Error: Missing filename", file=sys.stderr)
        print("Usage: edit FILENAME", file=sys.stderr)
        return 1
    
    # Find editor
    editor = os.environ.get('EDITOR')
    if not editor:
        # Try common editors
        for ed in ['nano', 'vi', 'vim']:
            if subprocess.run(['which', ed], capture_output=True).returncode == 0:
                editor = ed
                break
    
    if not editor:
        print("Error: No editor found. Set $EDITOR environment variable.", file=sys.stderr)
        return 1
    
    # Download file
    print(f"Downloading {filename}...")
    data = client.cat(filename)
    if data is None:
        # File doesn't exist - create new
        data = b""
    
    # Create temp file
    with tempfile.NamedTemporaryFile(mode='wb', suffix=os.path.basename(filename), delete=False) as tmp:
        tmp.write(data)
        tmp_path = tmp.name
    
    try:
        # Get original mtime
        orig_mtime = os.path.getmtime(tmp_path)
        
        # Launch editor
        result = subprocess.run([editor, tmp_path])
        if result.returncode != 0:
            print(f"Error: Editor exited with code {result.returncode}", file=sys.stderr)
            return 1
        
        # Check if file was modified
        new_mtime = os.path.getmtime(tmp_path)
        if new_mtime == orig_mtime:
            print("File not modified")
            return 0
        
        # Upload modified file
        print(f"Uploading {filename}...")
        with open(tmp_path, 'rb') as f:
            new_data = f.read()
        
        if client.put(filename, new_data):
            print(f"Uploaded {len(new_data)} bytes")
            return 0
        else:
            print(f"Error: Cannot upload file", file=sys.stderr)
            return 1
    
    finally:
        # Clean up temp file
        try:
            os.unlink(tmp_path)
        except:
            pass


def cmd_help(client: Load81Client, command: Optional[str] = None) -> int:
    """Show help information"""
    if command:
        # Show detailed help for specific command
        help_text = {
            'cat': 'cat FILE [FILE...]\n  Display contents of one or more files',
            'cd': 'cd [DIRECTORY]\n  Change current directory (default: /)',
            'cp': 'cp SOURCE DEST\n  Copy files\n  Examples:\n    cp remote:/file.txt ./local.txt  (download)\n    cp ./local.txt remote:/file.txt  (upload)',
            'edit': 'edit FILENAME\n  Edit remote file with local editor ($EDITOR)',
            'help': 'help [COMMAND]\n  Show help information',
            'ls': 'ls [PATH]\n  List directory contents',
            'mkdir': 'mkdir DIRECTORY\n  Create directory',
            'repl': 'repl\n  Enter interactive Lua REPL',
            'rm': 'rm PATH [PATH...]\n  Delete files or directories',
            'rsync': 'rsync SOURCE DEST\n  Synchronize directories\n  Remote paths must start with /\n  Examples:\n    rsync /load81 ./backup  (download from remote)\n    rsync ./backup /load81  (upload to remote)',
        }
        
        if command in help_text:
            print(help_text[command])
        else:
            print(f"Unknown command: {command}", file=sys.stderr)
            return 1
    else:
        # Show general help
        print("LOAD81R - Remote Shell for PicoCalc")
        print()
        print("Available commands:")
        print("  cat FILE...       Display file contents")
        print("  cd [DIR]          Change directory")
        print("  cp SRC DST        Copy files (use remote: prefix)")
        print("  edit FILE         Edit file with local editor")
        print("  help [CMD]        Show help")
        print("  ls [PATH]         List directory")
        print("  mkdir DIR         Create directory")
        print("  repl              Interactive Lua REPL")
        print("  rm PATH...        Delete files/directories")
        print("  rsync SRC DST     Synchronize directories")
        print()
        print("  exit, quit        Exit shell")
        print()
        print("Use 'help COMMAND' for detailed information")
    
    return 0


def cmd_ls(client: Load81Client, path: Optional[str] = None) -> int:
    """List directory contents"""
    entries = client.ls(path)
    if entries is None:
        print(f"Error: Cannot list directory", file=sys.stderr)
        return 1
    
    if not entries:
        return 0
    
    # Format output
    for entry in entries:
        name = entry.get('name', '?')
        size = entry.get('size', 0)
        is_dir = entry.get('is_dir', False)
        
        if is_dir:
            print(f"drwxr-xr-x  {name}/")
        else:
            # Format size
            if size < 1024:
                size_str = f"{size}B"
            elif size < 1024 * 1024:
                size_str = f"{size/1024:.1f}K"
            else:
                size_str = f"{size/(1024*1024):.1f}M"
            
            print(f"-rw-r--r--  {size_str:>6}  {name}")
    
    return 0


def cmd_mkdir(client: Load81Client, path: str) -> int:
    """Create directory"""
    if not path:
        print("Error: Missing directory name", file=sys.stderr)
        print("Usage: mkdir DIRECTORY", file=sys.stderr)
        return 1
    
    if client.mkdir(path):
        return 0
    else:
        print(f"Error: Cannot create directory '{path}'", file=sys.stderr)
        return 1


def cmd_repl(client: Load81Client) -> int:
    """Interactive Lua REPL"""
    print("Lua REPL - Type .exit to quit")
    print()
    
    while True:
        try:
            line = input("lua> ")
        except (EOFError, KeyboardInterrupt):
            print()
            break
        
        if not line:
            continue
        
        # Check for special commands
        if line in ['.exit', '.quit']:
            break
        elif line == '.help':
            print("Special commands:")
            print("  .exit, .quit  Exit REPL")
            print("  .help         Show this help")
            continue
        
        # Execute Lua code
        result = client.repl(line)
        if result is not None:
            print(result)
    
    return 0


def cmd_rm(client: Load81Client, *paths) -> int:
    """Delete files or directories"""
    if not paths:
        print("Error: Missing path", file=sys.stderr)
        print("Usage: rm PATH [PATH...]", file=sys.stderr)
        return 1
    
    exit_code = 0
    for path in paths:
        if client.rm(path):
            print(f"Deleted: {path}")
        else:
            print(f"Error: Cannot delete '{path}'", file=sys.stderr)
            exit_code = 1
    
    return exit_code


def cmd_rsync(client: Load81Client, src: str, dst: str) -> int:
    """Synchronize directories"""
    if not src or not dst:
        print("Error: Missing source or destination", file=sys.stderr)
        print("Usage: rsync SOURCE DEST", file=sys.stderr)
        print("  rsync /remote/dir ./local  (download from remote)", file=sys.stderr)
        print("  rsync ./local /remote/dir  (upload to remote)", file=sys.stderr)
        return 1
    
    # Determine direction based on which path is absolute (starts with /)
    # Absolute paths are assumed to be remote paths
    src_is_remote = src.startswith('/')
    dst_is_remote = dst.startswith('/')
    
    if src_is_remote and dst_is_remote:
        print("Error: Both paths are remote (server-side copy not supported)", file=sys.stderr)
        return 1
    
    if not src_is_remote and not dst_is_remote:
        print("Error: Both paths are local. At least one must be a remote path (starting with /)", file=sys.stderr)
        print("Usage: rsync /remote/dir ./local  OR  rsync ./local /remote/dir", file=sys.stderr)
        return 1
    
    if src_is_remote:
        # Remote source - download
        return _rsync_download(client, src, dst)
    else:
        # Local source - upload
        return _rsync_upload(client, src, dst)


def _rsync_download(client: Load81Client, remote_path: str, local_path: str) -> int:
    """Download directory recursively"""
    print(f"Syncing {remote_path} -> {local_path}")
    
    # Create local directory
    os.makedirs(local_path, exist_ok=True)
    
    # List remote directory
    entries = client.ls(remote_path)
    if entries is None:
        print(f"Error: Cannot list remote directory", file=sys.stderr)
        return 1
    
    file_count = 0
    error_count = 0
    
    for entry in entries:
        name = entry['name']
        is_dir = entry['is_dir']
        
        remote_file = f"{remote_path}/{name}" if remote_path != "/" else f"/{name}"
        local_file = os.path.join(local_path, name)
        
        if is_dir:
            # Recursively sync subdirectory
            result = _rsync_download(client, remote_file, local_file)
            if result != 0:
                error_count += 1
        else:
            # Download file
            print(f"  {remote_file}")
            data = client.cat(remote_file)
            if data is None:
                error_msg = client.last_error if client.last_error else "Cannot read"
                print(f"  Error: {error_msg} {remote_file}", file=sys.stderr)
                error_count += 1
                continue
            
            try:
                with open(local_file, 'wb') as f:
                    f.write(data)
                file_count += 1
            except IOError as e:
                print(f"  Error: Cannot write {local_file}: {e}", file=sys.stderr)
                error_count += 1
    
    print(f"Downloaded {file_count} files")
    return 1 if error_count > 0 else 0


def _rsync_upload(client: Load81Client, local_path: str, remote_path: str) -> int:
    """Upload directory recursively"""
    print(f"Syncing {local_path} -> {remote_path}")
    
    if not os.path.exists(local_path):
        print(f"Error: Local path does not exist: {local_path}", file=sys.stderr)
        return 1
    
    file_count = 0
    error_count = 0
    
    if os.path.isfile(local_path):
        # Upload single file
        try:
            with open(local_path, 'rb') as f:
                data = f.read()
            
            if client.put(remote_path, data):
                print(f"  {local_path}")
                file_count += 1
            else:
                print(f"  Error: Cannot upload {local_path}", file=sys.stderr)
                error_count += 1
        except IOError as e:
            print(f"  Error: Cannot read {local_path}: {e}", file=sys.stderr)
            error_count += 1
    
    elif os.path.isdir(local_path):
        # Create remote directory
        if not client.mkdir(remote_path):
            # Directory might already exist - that's okay
            pass
        
        # Upload directory contents
        for name in os.listdir(local_path):
            local_file = os.path.join(local_path, name)
            remote_file = f"{remote_path}/{name}" if remote_path != "/" else f"/{name}"
            
            result = _rsync_upload(client, local_file, remote_file)
            if result != 0:
                error_count += 1
            else:
                file_count += 1
    
    if file_count > 0:
        print(f"Uploaded {file_count} files")
    
    return 1 if error_count > 0 else 0