# Copyright (C) 2024-2026 Intel Corporation
# SPDX-License-Identifier: MIT

set(UNITTEST_TARGET swgenlock_tests)

set(UNITY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/unity)
set(UNITY_SRC ${UNITY_DIR}/src/unity.c)

if(NOT EXISTS ${UNITY_SRC})
    message(FATAL_ERROR
        "Unity test framework not found. Clone with: git clone https://github.com/ThrowTheSwitch/Unity.git unittest/unity --branch v2.6.1 --depth 1")
endif()

set(UNITTEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/test_genlock.c
    ${UNITY_SRC}
)

add_executable(${UNITTEST_TARGET} ${UNITTEST_SOURCES})

target_include_directories(${UNITTEST_TARGET}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${UNITY_DIR}/src
        ${APP_INCLUDE_DIRS}
)

target_link_libraries(${UNITTEST_TARGET}
    PRIVATE
        vsyncalter::shared
        ${PLATFORM_LIBS}
)

# Compile C files as C++ to handle version.h C++ includes
set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/test_genlock.c
    ${UNITY_SRC}
    PROPERTIES LANGUAGE CXX
)

set_target_properties(${UNITTEST_TARGET} PROPERTIES
    LINKER_LANGUAGE CXX
)

target_compile_options(${UNITTEST_TARGET} PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
)

if(UNITTEST_ENABLE_COVERAGE)
    target_compile_options(${UNITTEST_TARGET} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-fprofile-arcs -ftest-coverage -O0 -g>
    )
    target_link_options(${UNITTEST_TARGET} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-fprofile-arcs -ftest-coverage>
    )
endif()