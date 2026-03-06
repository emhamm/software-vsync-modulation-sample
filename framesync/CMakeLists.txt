# Copyright (C) 2025-2026 Intel Corporation
# SPDX-License-Identifier: MIT

# Can be built standalone or as part of main project
if(NOT CMAKE_PROJECT_NAME STREQUAL "vsyncalter")
	cmake_minimum_required(VERSION 3.12)
	project(framesync VERSION 1.0.0 LANGUAGES CXX)

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

# framesync executable
set(FRAMESYNC_TARGET framesync)

# Find required packages
find_package(SDL2 REQUIRED)

# Source files
set(FRAMESYNC_SOURCES
	main.cpp
	primary.cpp
	secondary.cpp
	renderer.cpp
)

# Headers
set(FRAMESYNC_HEADERS
	demo_protocol.h
	demo_common.h
	renderer.h
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
add_executable(${FRAMESYNC_TARGET} ${FRAMESYNC_SOURCES} ${OS_SOURCES})

# Link libraries
if(CMAKE_PROJECT_NAME STREQUAL "vsyncalter")
	# Integrated build - link with static library from main project
	target_link_libraries(${FRAMESYNC_TARGET} PRIVATE
		vsyncalter::static
		${PLATFORM_LIBS}
		${SDL2_LIBRARIES}
	)
else()
	# Standalone build - link with installed library
	target_link_libraries(${FRAMESYNC_TARGET} PRIVATE
		${VSYNCALTER_LIBRARY}
		${PLATFORM_LIBS}
		${SDL2_LIBRARIES}
	)
endif()

# Platform-specific threading library
if(PLATFORM STREQUAL "linux")
	find_package(Threads REQUIRED)
	target_link_libraries(${FRAMESYNC_TARGET} PRIVATE Threads::Threads)
endif()

# Include directories
target_include_directories(${FRAMESYNC_TARGET}
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		${APP_INCLUDE_DIRS}
		${SDL2_INCLUDE_DIRS}
)

# Compiler options
target_compile_options(${FRAMESYNC_TARGET} PRIVATE
	-Wall
	-Wextra
	-std=c++11
)

# Installation
install(TARGETS ${FRAMESYNC_TARGET}
	RUNTIME DESTINATION bin
)
