#!/usr/bin/env bash
# Copyright (C) 2025-2026 Intel Corporation
# SPDX-License-Identifier: MIT

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/builddir"
BUILD_TYPE="${1:-release}"

echo "=================================================="
echo "Building vsyncalter with Meson (Linux)"
echo "=================================================="
echo "Build type: ${BUILD_TYPE}"
echo ""

# Clean existing build directory if requested
if [[ "${2:-}" == "clean" ]]; then
    echo "Cleaning existing build directory..."
    rm -rf "${BUILD_DIR}"
fi

# Configure if needed
if [[ ! -d "${BUILD_DIR}" ]]; then
    echo "Configuring..."
    meson setup "${BUILD_DIR}" \
        --buildtype="${BUILD_TYPE}" \
        --prefix=/usr/local \
        -Dbuild_shared_libs=true \
        -Dbuild_swgenlock=true \
        -Dbuild_pllctl=true \
        -Dbuild_vblmon=true \
        -Dbuild_framesync=true \
        -Denable_coverage=false
    echo ""
else
    echo "Build directory exists. Ensuring code coverage is disabled..."
    meson configure "${BUILD_DIR}" -Denable_coverage=false
    echo ""
fi

# Build
echo "Building..."
meson compile -C "${BUILD_DIR}"

echo ""
echo "=================================================="
echo "Build complete!"
echo "=================================================="
echo "Binaries location:"
echo "  - Library:    ${BUILD_DIR}/lib/"
echo "  - pllctl:     ${BUILD_DIR}/pllctl/pllctl"
echo "  - vblmon:     ${BUILD_DIR}/vblmon/vblmon"
echo "  - swgenlock:  ${BUILD_DIR}/swgenlock/swgenlock"
echo "  - framesync:  ${BUILD_DIR}/framesync/framesync"
