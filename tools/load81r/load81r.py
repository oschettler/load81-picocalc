#!/usr/bin/env python3
"""
LOAD81R - Remote Shell for PicoCalc
Main entry point
"""

import sys
import argparse
from client import Load81Client
from shell import run_shell
from commands import (
    cmd_cat, cmd_cd, cmd_cp, cmd_edit, cmd_help,
    cmd_ls, cmd_mkdir, cmd_repl, cmd_rm, cmd_rsync, cmd_sshot
)


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description='LOAD81R - Remote Shell for PicoCalc',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s 192.168.1.100                    # Interactive shell
  %(prog)s 192.168.1.100 ls /load81         # List directory
  %(prog)s 192.168.1.100 cat /load81/nex.lua  # Display file
  %(prog)s 192.168.1.100 cp remote:/file.txt ./local.txt  # Download
  %(prog)s 192.168.1.100 cp ./local.txt remote:/file.txt  # Upload
  %(prog)s 192.168.1.100 rsync /load81 ./backup  # Download directory
  %(prog)s 192.168.1.100 rsync ./backup /load81  # Upload directory
  %(prog)s 192.168.1.100 sshot screenshot.png  # Capture screenshot
        """
    )
    
    parser.add_argument('host',
                       help='PicoCalc hostname or IP address')
    
    parser.add_argument('command',
                       nargs='?',
                       help='Command to execute (omit for interactive shell)')
    
    parser.add_argument('args',
                       nargs='*',
                       help='Command arguments')
    
    parser.add_argument('-p', '--port',
                       type=int,
                       default=1900,
                       help='Server port (default: 1900)')
    
    args = parser.parse_args()
    
    # Interactive shell mode
    if not args.command:
        return run_shell(args.host, args.port)
    
    # Command mode
    client = Load81Client()
    
    try:
        # Connect to server
        if not client.connect(args.host, args.port):
            print(f"Error: Cannot connect to {args.host}:{args.port}", file=sys.stderr)
            return 1
        
        # Execute command
        cmd = args.command
        cmd_args = args.args
        
        if cmd == 'cat':
            return cmd_cat(client, *cmd_args)
        
        elif cmd == 'cd':
            path = cmd_args[0] if cmd_args else None
            return cmd_cd(client, path)
        
        elif cmd == 'cp':
            if len(cmd_args) < 2:
                print("Error: Missing arguments", file=sys.stderr)
                print("Usage: cp SOURCE DEST", file=sys.stderr)
                return 1
            return cmd_cp(client, cmd_args[0], cmd_args[1])
        
        elif cmd == 'edit':
            if not cmd_args:
                print("Error: Missing filename", file=sys.stderr)
                return 1
            return cmd_edit(client, cmd_args[0])
        
        elif cmd == 'help':
            topic = cmd_args[0] if cmd_args else None
            return cmd_help(client, topic)
        
        elif cmd == 'ls':
            path = cmd_args[0] if cmd_args else None
            return cmd_ls(client, path)
        
        elif cmd == 'mkdir':
            if not cmd_args:
                print("Error: Missing directory name", file=sys.stderr)
                return 1
            return cmd_mkdir(client, cmd_args[0])
        
        elif cmd == 'repl':
            return cmd_repl(client)
        
        elif cmd == 'rm':
            if not cmd_args:
                print("Error: Missing path", file=sys.stderr)
                return 1
            return cmd_rm(client, *cmd_args)
        
        elif cmd == 'rsync':
            if len(cmd_args) < 2:
                print("Error: Missing arguments", file=sys.stderr)
                print("Usage: rsync SOURCE DEST", file=sys.stderr)
                return 1
            return cmd_rsync(client, cmd_args[0], cmd_args[1])
        
        elif cmd == 'sshot':
            if not cmd_args:
                print("Error: Missing filename", file=sys.stderr)
                return 1
            return cmd_sshot(client, cmd_args[0])
        
        else:
            print(f"Unknown command: {cmd}", file=sys.stderr)
            print("Use 'help' to see available commands", file=sys.stderr)
            return 1
    
    except KeyboardInterrupt:
        print()
        return 130
    
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1
    
    finally:
        client.close()


if __name__ == '__main__':
    sys.exit(main())