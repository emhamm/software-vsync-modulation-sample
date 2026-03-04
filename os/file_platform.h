/*
 * Copyright © 2024-2026 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#ifndef FILE_PLATFORM_H
#define FILE_PLATFORM_H

#ifdef __linux__
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>

    // Linux file handle type
    typedef int file_handle_t;
    #define INVALID_FILE_HANDLE (-1)

    // File open flags
    #define O_RDWR_PLATFORM      O_RDWR
    #define O_CLOEXEC_PLATFORM   O_CLOEXEC
#endif

/**
 * @brief Open a file/device with the specified path and flags
 * @param path - Path to the file/device
 * @param flags - File open flags (platform-specific)
 * @return File handle, or INVALID_FILE_HANDLE on error
 */
file_handle_t os_open_file(const char* path, int flags);

/**
 * @brief Close a file handle
 * @param handle - File handle to close
 * @return 0 on success, -1 on error
 */
int os_close_file(file_handle_t handle);

/**
 * @brief Check if a file exists and get its stats
 * @param path - Path to the file
 * @return 0 if file exists, -1 otherwise
 */
int os_file_exists(const char* path);

/**
 * @brief Get last error message
 * @param buffer - Buffer to store error message
 * @param size - Size of buffer
 * @return Pointer to error message
 */
const char* os_get_error(char* buffer, size_t size);

#endif // FILE_PLATFORM_H
