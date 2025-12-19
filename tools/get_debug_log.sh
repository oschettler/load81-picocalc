#!/bin/bash
# Retrieve debug log from PicoCalc diagnostic server
# Usage: ./get_debug_log.sh [ip_address]

IP="${1:-192.168.178.122}"
PORT=1901

echo "Connecting to PicoCalc diagnostic server at $IP:$PORT..."
echo

# Send any request (server responds to any input ending with newline)
echo "status" | nc -w 2 "$IP" "$PORT"

echo
echo "---"
echo "Debug log retrieved from diagnostic server"