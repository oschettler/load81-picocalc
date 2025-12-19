#!/usr/bin/env python3
"""
LOAD81R Interactive Shell
Command-line interface with readline support
"""

import sys
import readline
import atexit
import os
from typing import Optional
from client import Load81Client
from commands import (
    cmd_cat, cmd_cd, cmd_cp, cmd_edit, cmd_help,
    cmd_ls, cmd_mkdir, cmd_repl, cmd_rm, cmd_rsync
)


class Load81Shell:
    """Interactive shell for LOAD81R"""
    
    def __init__(self, host: str, port: int = 1900):
        self.host = host
        self.port = port
        self.client = Load81Client(host, port)
        self.running = False
        self.current_dir = "/"
        
        # Setup readline
        self._setup_readline()
    
    def _setup_readline(self):
        """Configure readline for command history and completion"""
        # History file
        history_file = os.path.expanduser("~/.load81r_history")
        try:
            readline.read_history_file(history_file)
        except FileNotFoundError:
            pass
        
        # Save history on exit
        atexit.register(readline.write_history_file, history_file)
        
        # Set history length
        readline.set_history_length(1000)
        
        # Enable tab completion
        readline.parse_and_bind("tab: complete")
        readline.set_completer(self._completer)
    
    def _completer(self, text: str, state: int):
        """Tab completion for commands and paths"""
        # Get current line
        line = readline.get_line_buffer()
        tokens = line.split()
        
        # Complete command names
        if not tokens or (len(tokens) == 1 and not line.endswith(' ')):
            commands = ['cat', 'cd', 'cp', 'edit', 'help', 'ls', 
                       'mkdir', 'repl', 'rm', 'rsync', 'exit', 'quit']
            matches = [cmd for cmd in commands if cmd.startswith(text)]
            return matches[state] if state < len(matches) else None
        
        # Complete file paths
        # TODO: Implement remote path completion
        return None
    
    def _get_prompt(self) -> str:
        """Generate shell prompt"""
        return f"load81r:{self.current_dir}> "
    
    def _parse_command(self, line: str) -> tuple:
        """Parse command line into command and arguments"""
        tokens = line.strip().split()
        if not tokens:
            return None, []
        return tokens[0], tokens[1:]
    
    def run(self) -> int:
        """Run interactive shell"""
        # Connect to server
        print(f"Connecting to {self.host}:{self.port}...")
        if not self.client.connect(self.host, self.port):
            print(f"Error: Cannot connect to {self.host}:{self.port}", file=sys.stderr)
            return 1
        
        print("Connected to PicoCalc")
        print("Type 'help' for available commands")
        print()
        
        # Update current directory
        pwd = self.client.pwd()
        if pwd:
            self.current_dir = pwd
        
        self.running = True
        exit_code = 0
        
        try:
            while self.running:
                try:
                    # Get command
                    line = input(self._get_prompt())
                    
                    # Parse command
                    cmd, args = self._parse_command(line)
                    if not cmd:
                        continue
                    
                    # Execute command
                    result = self._execute_command(cmd, args)
                    if result is not None:
                        exit_code = result
                
                except EOFError:
                    print()
                    break
                
                except KeyboardInterrupt:
                    print()
                    continue
        
        finally:
            self.client.close()
        
        return exit_code
    
    def _execute_command(self, cmd: str, args: list) -> Optional[int]:
        """Execute a shell command"""
        # Handle exit commands
        if cmd in ['exit', 'quit']:
            self.running = False
            return 0
        
        # Dispatch to command handlers
        try:
            if cmd == 'cat':
                return cmd_cat(self.client, *args)
            
            elif cmd == 'cd':
                path = args[0] if args else None
                result = cmd_cd(self.client, path)
                if result == 0:
                    # Update current directory
                    pwd = self.client.pwd()
                    if pwd:
                        self.current_dir = pwd
                return result
            
            elif cmd == 'cp':
                if len(args) < 2:
                    print("Error: Missing arguments", file=sys.stderr)
                    print("Usage: cp SOURCE DEST", file=sys.stderr)
                    return 1
                return cmd_cp(self.client, args[0], args[1])
            
            elif cmd == 'edit':
                if not args:
                    print("Error: Missing filename", file=sys.stderr)
                    return 1
                return cmd_edit(self.client, args[0])
            
            elif cmd == 'help':
                topic = args[0] if args else None
                return cmd_help(self.client, topic)
            
            elif cmd == 'ls':
                path = args[0] if args else None
                return cmd_ls(self.client, path)
            
            elif cmd == 'mkdir':
                if not args:
                    print("Error: Missing directory name", file=sys.stderr)
                    return 1
                return cmd_mkdir(self.client, args[0])
            
            elif cmd == 'repl':
                return cmd_repl(self.client)
            
            elif cmd == 'rm':
                if not args:
                    print("Error: Missing path", file=sys.stderr)
                    return 1
                return cmd_rm(self.client, *args)
            
            elif cmd == 'rsync':
                if len(args) < 2:
                    print("Error: Missing arguments", file=sys.stderr)
                    print("Usage: rsync SOURCE DEST", file=sys.stderr)
                    return 1
                return cmd_rsync(self.client, args[0], args[1])
            
            else:
                print(f"Unknown command: {cmd}", file=sys.stderr)
                print("Type 'help' for available commands", file=sys.stderr)
                return 1
        
        except Exception as e:
            print(f"Error: {e}", file=sys.stderr)
            return 1


def run_shell(host: str, port: int = 1900) -> int:
    """Run interactive shell"""
    shell = Load81Shell(host, port)
    return shell.run()