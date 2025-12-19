#!/bin/bash

# 9P protocol session test - keeps connection open
# Sends multiple messages in sequence over same connection

TARGET_IP="192.168.178.122"
TARGET_PORT="564"

echo "=========================================="
echo "9P Session Test (persistent connection)"
echo "=========================================="
echo "Target: $TARGET_IP:$TARGET_PORT"
echo ""

# Create a named pipe for bidirectional communication
PIPE="/tmp/9p_test_$$"
mkfifo "$PIPE"

# Start nc in background, reading from pipe
nc "$TARGET_IP" "$TARGET_PORT" < "$PIPE" | xxd &
NC_PID=$!

# Open pipe for writing
exec 3>"$PIPE"

sleep 0.5

echo "=== Sending Tversion ==="
# Tversion: size=19, type=100, tag=NOTAG, msize=8192, version="9P2000.u"
echo "1300000064ffff0020000008003950323030302e75" | xxd -r -p >&3
sleep 1

echo ""
echo "=== Sending Tattach ==="
# Tattach: size=32, type=104, tag=1, fid=1, afid=NOFID, uname="user", aname="/"
echo "2000000068010001000000ffffffff04007573657201002f" | xxd -r -p >&3
sleep 1

echo ""
echo "=== Closing connection ==="
exec 3>&-
wait $NC_PID
rm "$PIPE"

echo ""
echo "=========================================="
echo "Session test complete"
echo "=========================================="