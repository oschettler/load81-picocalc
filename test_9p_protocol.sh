#!/bin/bash
# Test 9P Protocol Handshake
# This script sends a Tversion message and displays the response

PICOCALC_IP="192.168.178.122"
PORT=564

echo "=========================================="
echo "9P Protocol Test - Tversion Message"
echo "=========================================="
echo "Target: $PICOCALC_IP:$PORT"
echo ""

# Tversion message structure:
# size[4] = 19 bytes (0x13 0x00 0x00 0x00)
# type[1] = 100 (0x64) - Tversion
# tag[2] = 65535 (0xff 0xff) - NOTAG
# msize[4] = 8192 (0x00 0x20 0x00 0x00)
# version[s] = "9P2000.u" (len=8: 0x08 0x00, then string)

echo "Sending Tversion message..."
echo "  size: 19 bytes"
echo "  type: 100 (Tversion)"
echo "  tag: 65535 (NOTAG)"
echo "  msize: 8192"
echo "  version: '9P2000.u'"
echo ""

# Send the message and capture response
RESPONSE=$(printf '\x13\x00\x00\x00\x64\xff\xff\x00\x20\x00\x00\x08\x00\x39\x50\x32\x30\x30\x30\x2e\x75' | nc -w 2 $PICOCALC_IP $PORT | xxd)

if [ -z "$RESPONSE" ]; then
    echo "❌ ERROR: No response received!"
    echo ""
    echo "Possible causes:"
    echo "  1. Server accepted connection but didn't respond"
    echo "  2. Server closed connection immediately"
    echo "  3. Response timeout (2 seconds)"
    echo ""
    echo "Try connecting with nc to see connection behavior:"
    echo "  nc $PICOCALC_IP $PORT"
    exit 1
fi

echo "✓ Response received!"
echo ""
echo "Raw response (hex dump):"
echo "$RESPONSE"
echo ""

# Parse response
FIRST_LINE=$(echo "$RESPONSE" | head -n 1)
SIZE_HEX=$(echo "$FIRST_LINE" | awk '{print $2$3$4$5}')
TYPE_HEX=$(echo "$FIRST_LINE" | awk '{print $6}')

echo "Parsed response:"
echo "  Size (hex): $SIZE_HEX"
echo "  Type (hex): $TYPE_HEX"

# Check if type is 101 (Rversion) or 107 (Rerror)
if [ "$TYPE_HEX" = "65" ]; then
    echo "  Type: 101 (Rversion) ✓ Correct!"
    echo ""
    echo "✓ Version negotiation successful!"
elif [ "$TYPE_HEX" = "6b" ]; then
    echo "  Type: 107 (Rerror) - Server returned error"
    echo ""
    echo "Server rejected version negotiation"
else
    echo "  Type: $TYPE_HEX (unexpected)"
    echo ""
    echo "❌ Unexpected response type"
fi

echo ""
echo "=========================================="