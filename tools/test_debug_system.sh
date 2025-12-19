#!/bin/bash
# Test script to verify debug system is working

if [ -z "$1" ]; then
    echo "Usage: $0 <picocalc-ip>"
    echo "Example: $0 192.168.178.122"
    exit 1
fi

PICOCALC_IP="$1"

echo "=== Testing PicoCalc Debug System ==="
echo
echo "1. Testing file server (port 1900)..."
echo "HELLO" | timeout 2 nc "$PICOCALC_IP" 1900
echo

echo "2. Testing diagnostic server (port 1901)..."
echo "status" | timeout 2 nc "$PICOCALC_IP" 1901
echo

echo "3. Checking for debug output in response..."
echo "status" | timeout 2 nc "$PICOCALC_IP" 1901 | grep -A 20 "Debug Log"
echo

echo "=== Test Complete ==="
echo
echo "Expected results:"
echo "  - File server should respond with: +OK load81r/1.0"
echo "  - Diagnostic server should show system status"
echo "  - Debug log section should contain boot messages"
echo
echo "If debug log is empty, the firmware may need to be rebuilt."