#!/usr/bin/env python3
"""
LOAD81R Protocol Client
Handles TCP communication with PicoCalc file server
"""

import socket
import sys
import json
from dataclasses import dataclass
from typing import Optional, List, Dict, Any


@dataclass
class Response:
    """Server response"""
    success: bool
    data: Optional[str] = None
    error: Optional[str] = None
    binary: Optional[bytes] = None


class Load81Client:
    """LOAD81R protocol client"""
    
    def __init__(self, host: Optional[str] = None, port: int = 1900):
        self.sock = None
        self.host = host
        self.port = port
        self.connected = False
        self.current_dir = "/"
        self.last_error = None
        
    def connect(self, host: str, port: int = 1900, timeout: float = 30.0) -> bool:
        """
        Connect to LOAD81R server
        
        Args:
            host: Server hostname or IP address
            port: Server port (default: 1900)
            timeout: Connection timeout in seconds (default: 30s for slow SD card reads)
            
        Returns:
            True if connected successfully, False otherwise
        """
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.settimeout(timeout)
            self.sock.connect((host, port))
            self.host = host
            self.port = port
            
            # Send HELLO handshake
            response = self.send_command("HELLO", "load81r/1.0")
            if not response.success:
                self.close()
                return False
            
            # Get initial directory
            pwd_response = self.send_command("PWD")
            if pwd_response.success and pwd_response.data:
                self.current_dir = pwd_response.data
            
            self.connected = True
            return True
            
        except (socket.error, socket.timeout) as e:
            if self.sock:
                self.sock.close()
                self.sock = None
            return False
    
    def close(self):
        """Close connection to server"""
        if self.sock:
            try:
                self.send_command("QUIT")
            except:
                pass
            self.sock.close()
            self.sock = None
        self.connected = False
    
    def send_command(self, cmd: str, *args) -> Response:
        """
        Send command to server and receive response
        
        Args:
            cmd: Command name
            *args: Command arguments
            
        Returns:
            Response object
        """
        if not self.sock:
            return Response(success=False, error="Not connected")
        
        try:
            # Format command
            if args:
                command_line = f"{cmd} {' '.join(str(arg) for arg in args)}\n"
            else:
                command_line = f"{cmd}\n"
            
            # Send command
            self.sock.sendall(command_line.encode('utf-8'))
            
            # Receive response
            return self._receive_response()
            
        except (socket.error, socket.timeout) as e:
            return Response(success=False, error=f"Communication error: {e}")
    
    def _receive_response(self) -> Response:
        """
        Receive and parse server response
        
        Returns:
            Response object
        """
        try:
            # Read response line
            line = self._read_line()
            if not line:
                return Response(success=False, error="Empty response")
            
            # Parse response
            if line.startswith("+OK"):
                # Success response
                data = line[4:].strip() if len(line) > 4 else None
                return Response(success=True, data=data)
                
            elif line.startswith("-ERR"):
                # Error response
                error = line[5:].strip() if len(line) > 5 else "Unknown error"
                return Response(success=False, error=error)
                
            elif line.startswith("+DATA"):
                # Binary data response
                # Parse length
                parts = line.split()
                if len(parts) < 2:
                    return Response(success=False, error="Invalid DATA response")
                
                try:
                    length = int(parts[1])
                except ValueError:
                    return Response(success=False, error="Invalid DATA length")
                
                # Read binary data
                data = self._read_bytes(length)
                
                # Read +END marker
                end_line = self._read_line()
                if not end_line or not end_line.startswith("+END"):
                    return Response(success=False, error="Missing END marker")
                
                return Response(success=True, binary=data)
                
            elif line.startswith("+READY"):
                # Ready for data upload
                return Response(success=True, data="READY")
                
            else:
                return Response(success=False, error=f"Unknown response: {line}")
                
        except (socket.error, socket.timeout) as e:
            return Response(success=False, error=f"Receive error: {e}")
    
    def _read_line(self) -> str:
        """Read a line from socket (terminated by \\n)"""
        line = b""
        while True:
            char = self.sock.recv(1)
            if not char:
                break
            if char == b'\n':
                break
            line += char
        return line.decode('utf-8', errors='replace').strip()
    
    def _read_bytes(self, length: int) -> bytes:
        """Read exact number of bytes from socket"""
        data = b""
        remaining = length
        while remaining > 0:
            chunk = self.sock.recv(min(remaining, 8192))
            if not chunk:
                break
            data += chunk
            remaining -= len(chunk)
        return data
    
    def send_data(self, data: bytes) -> bool:
        """
        Send binary data to server (after PUT command)
        
        Args:
            data: Binary data to send
            
        Returns:
            True if sent successfully, False otherwise
        """
        if not self.sock:
            return False
        
        try:
            self.sock.sendall(data)
            return True
        except (socket.error, socket.timeout):
            return False
    
    # High-level command methods
    
    def pwd(self) -> Optional[str]:
        """Get current working directory"""
        response = self.send_command("PWD")
        if response.success:
            self.current_dir = response.data or "/"
            return self.current_dir
        return None
    
    def cd(self, path: str) -> bool:
        """Change directory"""
        response = self.send_command("CD", path)
        if response.success:
            # Update current directory
            self.pwd()
            return True
        return False
    
    def ls(self, path: Optional[str] = None) -> Optional[List[Dict[str, Any]]]:
        """List directory contents"""
        if path:
            response = self.send_command("LS", path)
        else:
            response = self.send_command("LS")
        
        if response.success and response.binary:
            try:
                json_str = response.binary.decode('utf-8')
                return json.loads(json_str)
            except (json.JSONDecodeError, UnicodeDecodeError):
                return None
        return None
    
    def cat(self, path: str) -> Optional[bytes]:
        """Read file contents"""
        response = self.send_command("CAT", path)
        if response.success and response.binary:
            return response.binary
        # Store last error for debugging
        if response.error:
            self.last_error = response.error
        return None
    
    def put(self, path: str, data: bytes) -> bool:
        """Write file"""
        # Send PUT command with size
        response = self.send_command("PUT", path, len(data))
        if not response.success:
            print(f"DEBUG: PUT command failed: {response.error}", file=sys.stderr)
            return False
        if response.data != "READY":
            print(f"DEBUG: Expected READY, got: {response.data}", file=sys.stderr)
            return False
        
        # Send data
        if not self.send_data(data):
            print(f"DEBUG: Failed to send data", file=sys.stderr)
            return False
        
        # Wait for confirmation
        response = self._receive_response()
        if not response.success:
            print(f"DEBUG: Upload confirmation failed: {response.error}", file=sys.stderr)
        return response.success
    
    def mkdir(self, path: str) -> bool:
        """Create directory"""
        response = self.send_command("MKDIR", path)
        return response.success
    
    def rm(self, path: str) -> bool:
        """Delete file or directory"""
        response = self.send_command("RM", path)
        return response.success
    
    def stat(self, path: str) -> Optional[Dict[str, Any]]:
        """Get file/directory information"""
        response = self.send_command("STAT", path)
        if response.success and response.data:
            try:
                return json.loads(response.data)
            except json.JSONDecodeError:
                return None
        return None
    
    def repl(self, code: str) -> Optional[str]:
        """Execute Lua code"""
        response = self.send_command("REPL", code)
        if response.success:
            return response.data
        return None
    
    def ping(self) -> bool:
        """Ping server"""
        response = self.send_command("PING")
        return response.success
    
    def __enter__(self):
        """Context manager entry"""
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit"""
        self.close()
        return False