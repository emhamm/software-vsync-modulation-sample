#!/bin/bash
# Copyright (C) 2024-2026 Intel Corporation
# SPDX-License-Identifier: MIT

# Build script for unittest using CMake

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$SCRIPT_DIR/build"

# Check if Unity is cloned
if [ ! -f "$SCRIPT_DIR/unity/src/unity.c" ]; then
    echo "Unity test framework not found."
    echo "Cloning Unity framework..."
    git clone https://github.com/ThrowTheSwitch/Unity.git "$SCRIPT_DIR/unity" --branch v2.6.1 --depth 1
fi

# First, build the main library
echo "Building vsyncalter library..."
mkdir -p "$PROJECT_ROOT/build"
cd "$PROJECT_ROOT/build"
cmake .. -DUNITTEST_ENABLE_COVERAGE=ON
cmake --build . --target vsyncalter_shared

# Build the applications with code coverage support
echo "Building applications with code coverage support..."
cmake --build . --target pllctl
cmake --build . --target swgenlock
cmake --build . --target vblmon

# Now build the unittest
echo "Building unit tests..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Create a minimal CMakeLists.txt for unittest standalone build
cat > CMakeLists.txt << 'EOF'
cmake_minimum_required(VERSION 3.15)
project(vsyncalter_unittest LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

# Import the library
add_library(vsyncalter_shared SHARED IMPORTED)
set_target_properties(vsyncalter_shared PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../../build/lib/libvsyncalter.so"
)

# Platform detection
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(PLATFORM "linux")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# Platform libraries
find_library(RT_LIBRARY rt REQUIRED)
find_library(DRM_LIBRARY drm REQUIRED)
find_library(PCIACCESS_LIBRARY pciaccess REQUIRED)
set(PLATFORM_LIBS ${RT_LIBRARY} ${DRM_LIBRARY} ${PCIACCESS_LIBRARY})

# Unity and test sources
set(UNITTEST_TARGET swgenlock_tests)
set(UNITY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../unity)
set(UNITY_SRC ${UNITY_DIR}/src/unity.c)

set(UNITTEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../test_genlock.c
    ${UNITY_SRC}
)

add_executable(${UNITTEST_TARGET} ${UNITTEST_SOURCES})

target_include_directories(${UNITTEST_TARGET}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
        ${CMAKE_CURRENT_SOURCE_DIR}/../../cmn
        ${CMAKE_CURRENT_SOURCE_DIR}/../../os
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${UNITY_DIR}/src
        /usr/include/drm
)

target_link_libraries(${UNITTEST_TARGET}
    PRIVATE
        vsyncalter_shared
        ${PLATFORM_LIBS}
)

# Compile C files as C++ to handle version.h C++ includes
set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/../test_genlock.c
    ${UNITY_SRC}
    PROPERTIES LANGUAGE CXX
)

set_target_properties(${UNITTEST_TARGET} PROPERTIES
    LINKER_LANGUAGE CXX
    BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/../../build/lib"
    INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/../../build/lib"
)

target_compile_options(${UNITTEST_TARGET} PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -fprofile-arcs -ftest-coverage -O0 -g>
)

target_link_options(${UNITTEST_TARGET} PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-fprofile-arcs -ftest-coverage>
)
EOF

cmake .
cmake --build .

echo ""
echo "Unit test binary built at: $BUILD_DIR/swgenlock_tests"
echo "To run tests: $BUILD_DIR/swgenlock_tests"
echo "To run with M_N tests: $BUILD_DIR/swgenlock_tests --run-mn-test"
