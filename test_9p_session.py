#!/usr/bin/env python3
"""
9P Protocol Session Test
Tests Tversion and Tattach in a single persistent connection
"""

import socket
import sys
import time

TARGET_IP = "192.168.178.122"
TARGET_PORT = 564

def hex_dump(data, label=""):
    """Print hex dump of data"""
    if label:
        print(f"\n{label}:")
    hex_str = data.hex()
    # Print in groups of 2 bytes
    for i in range(0, len(hex_str), 32):
        chunk = hex_str[i:i+32]
        formatted = ' '.join(chunk[j:j+2] for j in range(0, len(chunk), 2))
        print(f"  {formatted}")
    print(f"  Length: {len(data)} bytes")

def parse_9p_header(data):
    """Parse 9P message header"""
    if len(data) < 7:
        return None
    size = int.from_bytes(data[0:4], 'little')
    msg_type = data[4]
    tag = int.from_bytes(data[5:7], 'little')
    return {'size': size, 'type': msg_type, 'tag': tag}

def main():
    print("=" * 50)
    print("9P Protocol Session Test")
    print("=" * 50)
    print(f"Target: {TARGET_IP}:{TARGET_PORT}\n")
    
    try:
        # Create socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5.0)
        
        print("Connecting...")
        sock.connect((TARGET_IP, TARGET_PORT))
        print("✓ Connected\n")
        
        # Test 1: Tversion
        print("=== Test 1: Tversion ===")
        tversion = bytes.fromhex("1300000064ffff0020000008003950323030302e75")
        hex_dump(tversion, "Sending Tversion")
        
        sock.sendall(tversion)
        time.sleep(0.5)
        
        response = sock.recv(4096)
        if response:
            hex_dump(response, "Received Rversion")
            header = parse_9p_header(response)
            if header:
                print(f"  Type: {header['type']} (expected 101)")
                print(f"  Tag: {header['tag']:#06x}")
        else:
            print("  ✗ No response received")
            return 1
        
        # Test 2: Tattach
        print("\n=== Test 2: Tattach ===")
        # Fixed size: 24 bytes (0x18) not 32 (0x20)
        tattach = bytes.fromhex("1800000068010001000000ffffffff04007573657201002f")
        hex_dump(tattach, "Sending Tattach")
        
        sock.sendall(tattach)
        time.sleep(0.5)
        
        response = sock.recv(4096)
        if response:
            hex_dump(response, "Received Rattach")
            header = parse_9p_header(response)
            if header:
                print(f"  Type: {header['type']} (expected 105 for Rattach or 107 for Rerror)")
                print(f"  Tag: {header['tag']:#06x}")
                
                if header['type'] == 107:  # Rerror
                    # Parse error string
                    if len(response) > 9:
                        str_len = int.from_bytes(response[7:9], 'little')
                        if len(response) >= 9 + str_len:
                            error_msg = response[9:9+str_len].decode('utf-8', errors='replace')
                            print(f"  Error: {error_msg}")
        else:
            print("  ✗ No response received")
            return 1
        
        print("\n" + "=" * 50)
        print("Session test complete")
        print("=" * 50)
        
        sock.close()
        return 0
        
    except socket.timeout:
        print("✗ Connection timeout")
        return 1
    except ConnectionRefusedError:
        print("✗ Connection refused")
        return 1
    except Exception as e:
        print(f"✗ Error: {e}")
        return 1

if __name__ == "__main__":
    sys.exit(main())