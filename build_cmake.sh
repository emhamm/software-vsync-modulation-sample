#!/bin/bash
# Copyright (C) 2024-2026 Intel Corporation
# SPDX-License-Identifier: MIT

# Build script for Linux using CMake

set -e

BUILD_DIR="build"
BUILD_TYPE="${1:-Release}"

echo "=================================================="
echo "Building vsyncalter with CMake (Linux)"
echo "=================================================="
echo "Build type: $BUILD_TYPE"
echo ""

# Create build directory
if [ -d "$BUILD_DIR" ]; then
    echo "Cleaning existing build directory..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure
echo "Configuring..."
cmake .. \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DBUILD_SHARED_LIBS=ON \
    -DBUILD_SWGENLOCK=ON \
    -DBUILD_PLLCTL=ON \
    -DBUILD_VBLMON=ON \
    -DBUILD_FRAMESYNC=ON

# Build
echo "Building..."
cmake --build . -- -j$(nproc)

echo ""
echo "=================================================="
echo "Build complete!"
echo "=================================================="
echo "Binaries location:"
echo "  - Library:    $BUILD_DIR/lib/"
echo "  - pllctl:     $BUILD_DIR/pllctl/pllctl"
echo "  - vblmon:     $BUILD_DIR/vblmon/vblmon"
echo "  - swgenlock:  $BUILD_DIR/swgenlock/swgenlock"
echo "  - framesync:  $BUILD_DIR/framesync/framesync"
echo ""
echo "To install: cd $BUILD_DIR && sudo make install"
echo "=================================================="
