# Copyright (C) 2024-2026 Intel Corporation
# SPDX-License-Identifier: MIT

# Can be built standalone or as part of main project
if(NOT CMAKE_PROJECT_NAME STREQUAL "vsyncalter")
    cmake_minimum_required(VERSION 3.15)
    project(pllctl VERSION 4.0.0 LANGUAGES CXX)

    # Standalone build - need to find vsyncalter
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    # Detect platform
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(PLATFORM "linux")
    else()
        message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
    endif()

    # Find vsyncalter library
    find_path(VSYNCALTER_INCLUDE_DIR vsyncalter.h
        PATHS /usr/local/include/vsyncalter /usr/include/vsyncalter
        PATH_SUFFIXES vsyncalter
    )
    find_library(VSYNCALTER_LIBRARY NAMES vsyncalter libvsyncalter
        PATHS /usr/local/lib /usr/lib
    )

    if(NOT VSYNCALTER_INCLUDE_DIR OR NOT VSYNCALTER_LIBRARY)
        message(FATAL_ERROR "vsyncalter library not found. Install it or build from root project.")
    endif()

    # Find platform libraries
    if(PLATFORM STREQUAL "linux")
        include_directories(/usr/include/drm)
        find_library(RT_LIBRARY rt REQUIRED)
        find_library(DRM_LIBRARY drm REQUIRED)
        find_library(PCIACCESS_LIBRARY pciaccess REQUIRED)
        set(PLATFORM_LIBS ${RT_LIBRARY} ${DRM_LIBRARY} ${PCIACCESS_LIBRARY})
    endif()

    # Set include directories for standalone build
    set(APP_INCLUDE_DIRS ${VSYNCALTER_INCLUDE_DIR})
endif()

# Synctest executable
set(PLLCTL_TARGET pllctl)

# Sources
file(GLOB PLLCTL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

# Platform-specific OS sources
if(CMAKE_PROJECT_NAME STREQUAL "vsyncalter")
    # Building as part of main project - use in-tree OS sources
    file(GLOB OS_SOURCES
        ${CMAKE_SOURCE_DIR}/os/${PLATFORM}/*.cpp
    )
else()
    # Standalone build - OS sources are linked in library
    set(OS_SOURCES "")
endif()

# Create executable
add_executable(${PLLCTL_TARGET} ${PLLCTL_SOURCES} ${OS_SOURCES})

# Link libraries - different for standalone vs subproject
if(CMAKE_PROJECT_NAME STREQUAL "vsyncalter")
    # Building as part of main project
    target_link_libraries(${PLLCTL_TARGET}
        PRIVATE
            vsyncalter::static
            ${PLATFORM_LIBS}
    )
else()
    # Standalone build
    target_link_libraries(${PLLCTL_TARGET}
        PRIVATE
            ${VSYNCALTER_LIBRARY}
            ${PLATFORM_LIBS}
    )
endif()

# Include directories
target_include_directories(${PLLCTL_TARGET}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${APP_INCLUDE_DIRS}
)

# Compiler options
target_compile_options(${PLLCTL_TARGET} PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# Code coverage support
if(UNITTEST_ENABLE_COVERAGE)
    target_compile_options(${PLLCTL_TARGET} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-fprofile-arcs -ftest-coverage -O0 -g>
    )
    target_link_options(${PLLCTL_TARGET} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-fprofile-arcs -ftest-coverage>
    )
endif()

# Installation
install(TARGETS ${PLLCTL_TARGET}
    RUNTIME DESTINATION bin
)
