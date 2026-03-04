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

#include "file_platform.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

file_handle_t os_open_file(const char* path, int flags) {
    if (!path) {
        return INVALID_FILE_HANDLE;
    }

    return open(path, flags, 0);
}

int os_close_file(file_handle_t handle) {
    if (handle == INVALID_FILE_HANDLE) {
        return -1;
    }

    return close(handle);
}

int os_file_exists(const char* path) {
    if (!path) {
        return -1;
    }

    struct stat st;
    return stat(path, &st);
}

const char* os_get_error(char* buffer, size_t size) {
    if (!buffer || size == 0) {
        return "Invalid buffer";
    }

    snprintf(buffer, size, "%s", strerror(errno));
    return buffer;
}
