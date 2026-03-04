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

#ifndef _MMIO_H
#define _MMIO_H

#include "mmio_interface.h"

// Backward compatibility macros - redirect to interface
#define READ_OFFSET_DWORD(x)  (get_mmio_interface()->read_reg(x))
#define WRITE_OFFSET_DWORD(x, y) (get_mmio_interface()->write_reg(x, y))
#define IS_INIT() (get_mmio_interface()->is_initialized())
#define INIT()    // No-op: initialization handled by interface
#define UNINIT()  // No-op: cleanup handled by interface

// Legacy function wrappers for backward compatibility
inline int map_mmio(const char *device_str) {
	return get_mmio_interface()->initialize(device_str);
}

inline int close_mmio_handle() {
	int result = get_mmio_interface()->cleanup();
	destroy_mmio_interface();
	return result;
}

inline int get_device_id(const char *device_str) {
	return get_mmio_interface()->get_device_id(device_str);
}

// Legacy global variable accessors (deprecated - use interface directly)
inline unsigned int get_cpu_offset() {
	return get_mmio_interface()->get_cpu_offset();
}

inline void* get_mmio_base() {
	return get_mmio_interface()->get_mmio_base();
}

inline int get_mmio_size() {
	return get_mmio_interface()->get_mmio_size();
}

#endif
