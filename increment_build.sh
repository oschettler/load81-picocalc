#!/bin/bash
# Script to increment build number and rebuild firmware

BUILD_FILE="src/build_version.h"

# Extract current build number
CURRENT_BUILD=$(grep "^#define BUILD_NUMBER" "$BUILD_FILE" | awk '{print $3}')

# Increment build number
NEW_BUILD=$((CURRENT_BUILD + 1))

echo "Incrementing build number: $CURRENT_BUILD -> $NEW_BUILD"

# Update build_version.h
sed -i "s/^#define BUILD_NUMBER.*/#define BUILD_NUMBER $NEW_BUILD/" "$BUILD_FILE"

echo "Updated $BUILD_FILE"
echo ""
echo "Building firmware..."
cd build && make -j$(nproc)

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Build successful!"
    echo "✓ Build number: $NEW_BUILD"
    echo "✓ Firmware: build/load81_picocalc.uf2"
    echo ""
    echo "To flash: cp build/load81_picocalc.uf2 /media/RPI-RP2/"
else
    echo ""
    echo "✗ Build failed!"
    exit 1
fi