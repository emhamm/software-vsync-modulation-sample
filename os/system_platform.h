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

#ifndef SYSTEM_PLATFORM_H
#define SYSTEM_PLATFORM_H

#include <stddef.h>
#include <stdint.h>
#include "shared_types.h"

// Device handle type (abstracted from file descriptor)
typedef int device_handle_t;
#define INVALID_DEVICE_HANDLE (-1)

// Platform-independent timespec structure
typedef struct {
	int64_t tv_sec;   // seconds
	int64_t tv_nsec;  // nanoseconds
} os_timespec;

// Clock types
typedef enum {
	OS_CLOCK_REALTIME,
	OS_CLOCK_MONOTONIC
} os_clock_type;

// System operations
device_handle_t os_open_device(const char* device_str);
void os_close_device(device_handle_t handle);
int os_make_timer(long expire_ms, void* user_ptr, reset_func callback, void** timer_id);
int os_stat_file(const char* path);
int os_clock_gettime(os_clock_type clock_id, os_timespec* tp);

#endif // SYSTEM_PLATFORM_H
