#!/bin/bash

# Comprehensive 9P protocol test
# Tests Tversion, Tauth, and Tattach messages

TARGET_IP="192.168.178.122"
TARGET_PORT="564"

echo "=========================================="
echo "9P Full Protocol Test"
echo "=========================================="
echo "Target: $TARGET_IP:$TARGET_PORT"
echo ""

# Function to send a message and receive response
send_9p_message() {
    local msg_hex="$1"
    local msg_name="$2"
    
    echo "Sending $msg_name..."
    echo "$msg_hex" | xxd -r -p | nc -w 2 "$TARGET_IP" "$TARGET_PORT" | xxd -p
}

# Test 1: Tversion
echo "=== Test 1: Tversion ==="
TVERSION="13000000 64ffff 00200000 08003950323030302e75"
TVERSION_CLEAN=$(echo $TVERSION | tr -d ' ')
echo "Request: $TVERSION_CLEAN"
RESPONSE=$(send_9p_message "$TVERSION_CLEAN" "Tversion")
echo "Response: $RESPONSE"
echo ""

# Test 2: Tauth (should return error "authentication not required")
echo "=== Test 2: Tauth ==="
# Tauth: size[4] type[1]=102 tag[2]=0 afid[4]=0 uname[2+n]="user" aname[2+n]="/"
TAUTH="1a000000 6600 00000000 0400757365 7201002f"
TAUTH_CLEAN=$(echo $TAUTH | tr -d ' ')
echo "Request: $TAUTH_CLEAN"
RESPONSE=$(send_9p_message "$TAUTH_CLEAN" "Tauth")
echo "Response: $RESPONSE"
echo ""

# Test 3: Tattach
echo "=== Test 3: Tattach ==="
# Tattach: size[4] type[1]=104 tag[2]=1 fid[4]=1 afid[4]=NOFID uname[2+n]="user" aname[2+n]="/"
TATTACH="20000000 6801 00 01000000 ffffffff 04007573 6572 01002f"
TATTACH_CLEAN=$(echo $TATTACH | tr -d ' ')
echo "Request: $TATTACH_CLEAN"
RESPONSE=$(send_9p_message "$TATTACH_CLEAN" "Tattach")
echo "Response: $RESPONSE"
echo ""

echo "=========================================="
echo "Test complete"
echo "=========================================="