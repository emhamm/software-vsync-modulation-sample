# Copyright (C) 2024-2026 Intel Corporation
# SPDX-License-Identifier: MIT

# Library target
set(LIB_NAME vsyncalter)

# Library sources
file(GLOB LIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

# Platform-specific OS sources
file(GLOB OS_SOURCES
    ${CMAKE_SOURCE_DIR}/os/${PLATFORM}/*.cpp
)

# All sources
set(ALL_SOURCES ${LIB_SOURCES} ${OS_SOURCES})

# Remove sources from list (if any platform-specific exclusions needed)
# For now, include all sources

# Library headers
file(GLOB LIB_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_SOURCE_DIR}/cmn/*.h
    ${CMAKE_SOURCE_DIR}/os/*.h
)

# Create shared and static libraries
add_library(${LIB_NAME}_shared SHARED ${ALL_SOURCES})
add_library(${LIB_NAME}_static STATIC ${ALL_SOURCES})

# Set output names
set_target_properties(${LIB_NAME}_shared PROPERTIES
    OUTPUT_NAME ${LIB_NAME}
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

set_target_properties(${LIB_NAME}_static PROPERTIES
    OUTPUT_NAME ${LIB_NAME}
)

# Include directories
target_include_directories(${LIB_NAME}_shared
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/cmn>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/os>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/os/${PLATFORM}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/platforms>
        $<INSTALL_INTERFACE:include>
)

target_include_directories(${LIB_NAME}_static
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/cmn>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/os>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/os/${PLATFORM}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/platforms>
        $<INSTALL_INTERFACE:include>
)

# Compiler options
target_compile_options(${LIB_NAME}_shared PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

target_compile_options(${LIB_NAME}_static PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# Code coverage support
if(UNITTEST_ENABLE_COVERAGE)
    target_compile_options(${LIB_NAME}_shared PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-fprofile-arcs -ftest-coverage -O0 -g>
    )
    target_link_options(${LIB_NAME}_shared PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-fprofile-arcs -ftest-coverage>
    )

    target_compile_options(${LIB_NAME}_static PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-fprofile-arcs -ftest-coverage -O0 -g>
    )
    target_link_options(${LIB_NAME}_static PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-fprofile-arcs -ftest-coverage>
    )
endif()

# Position Independent Code for shared library
set_target_properties(${LIB_NAME}_shared PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# Link libraries
target_link_libraries(${LIB_NAME}_shared
    PUBLIC ${PLATFORM_LIBS}
)

target_link_libraries(${LIB_NAME}_static
    PUBLIC ${PLATFORM_LIBS}
)

# Installation
install(TARGETS ${LIB_NAME}_shared ${LIB_NAME}_static
    EXPORT vsyncalterTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# Export targets
install(EXPORT vsyncalterTargets
    FILE vsyncalterTargets.cmake
    NAMESPACE vsyncalter::
    DESTINATION lib/cmake/vsyncalter
)

# Create alias targets for use within build tree
add_library(vsyncalter::shared ALIAS ${LIB_NAME}_shared)
add_library(vsyncalter::static ALIAS ${LIB_NAME}_static)
