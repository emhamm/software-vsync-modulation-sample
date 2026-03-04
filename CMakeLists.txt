# Copyright (C) 2024-2026 Intel Corporation
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.15)

project(vsyncalter
    VERSION 4.0.0
    DESCRIPTION "VSync Alteration Library and Tools"
    LANGUAGES CXX
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "System: ${CMAKE_SYSTEM_NAME}")

# Platform detection
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(PLATFORM "linux")
    message(STATUS "Detected platform: Linux")
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

# Options
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(BUILD_SWGENLOCK "Build swgenlock application" ON)
option(BUILD_PLLCTL "Build pllctl application" ON)
option(BUILD_VBLMON "Build vblmon application" ON)
option(BUILD_FRAMESYNC "Build framesync application" ON)
option(UNITTEST_ENABLE_COVERAGE "Enable code coverage instrumentation for unit tests" OFF)

# Global include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/cmn
    ${CMAKE_SOURCE_DIR}/os
    ${CMAKE_SOURCE_DIR}/lib
)

# Include directories for applications (without lib)
set(APP_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/cmn
    ${CMAKE_SOURCE_DIR}/os
)

# Platform-specific settings
if(PLATFORM STREQUAL "linux")
    # Linux-specific includes and libraries
    include_directories(/usr/include/drm)

    # Find required libraries
    find_library(RT_LIBRARY rt REQUIRED)
    find_library(DRM_LIBRARY drm REQUIRED)
    find_library(PCIACCESS_LIBRARY pciaccess REQUIRED)

    set(PLATFORM_LIBS ${RT_LIBRARY} ${DRM_LIBRARY} ${PCIACCESS_LIBRARY})

    message(STATUS "Linux libraries found:")
    message(STATUS "  - rt: ${RT_LIBRARY}")
    message(STATUS "  - drm: ${DRM_LIBRARY}")
    message(STATUS "  - pciaccess: ${PCIACCESS_LIBRARY}")

endif()

# Add subdirectories
add_subdirectory(lib)

if(BUILD_PLLCTL)
    add_subdirectory(pllctl)
endif()

if(BUILD_VBLMON)
    add_subdirectory(vblmon)
endif()

if(BUILD_SWGENLOCK)
    add_subdirectory(swgenlock)
endif()

if(BUILD_FRAMESYNC)
    add_subdirectory(framesync)
endif()

# Installation
install(DIRECTORY cmn/ DESTINATION include/vsyncalter
    FILES_MATCHING PATTERN "*.h")

install(DIRECTORY os/ DESTINATION include/vsyncalter/os
    FILES_MATCHING PATTERN "*.h")

# Print summary
message(STATUS "")
message(STATUS "==================================================")
message(STATUS "VSync Alteration Build Configuration Summary")
message(STATUS "==================================================")
message(STATUS "  Version:          ${PROJECT_VERSION}")
message(STATUS "  Platform:         ${PLATFORM}")
message(STATUS "  Build type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "  Build framesync:  ${BUILD_FRAMESYNC}")
message(STATUS "  Shared libraries: ${BUILD_SHARED_LIBS}")
message(STATUS "  Build pllctl:   ${BUILD_PLLCTL}")
message(STATUS "  Build vblmon:    ${BUILD_VBLMON}")
message(STATUS "  Build swgenlock:  ${BUILD_SWGENLOCK}")
message(STATUS "  Coverage enabled: ${UNITTEST_ENABLE_COVERAGE}")
message(STATUS "  Install prefix:   ${CMAKE_INSTALL_PREFIX}")
message(STATUS "==================================================")
message(STATUS "")
