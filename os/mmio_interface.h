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

#ifndef _MMIO_INTERFACE_H
#define _MMIO_INTERFACE_H

#include <stdint.h>

/**
 * @brief Abstract interface for Memory-Mapped I/O operations
 *
 * This interface provides platform-agnostic MMIO access.
 * Implementations exist for Linux (libpciaccess) and Windows (WDDM/KMD)
 */
class MMIOInterface {
public:
	virtual ~MMIOInterface() = default;

	// Initialization and cleanup
	virtual int initialize(const char* device_str) = 0;
	virtual int cleanup() = 0;
	virtual bool is_initialized() const = 0;

	// Device information
	virtual int get_device_id(const char* device_str) = 0;

	// Register access (32-bit DWORD operations)
	virtual uint32_t read_reg(uint64_t offset) = 0;
	virtual void write_reg(uint64_t offset, uint32_t value) = 0;

	// Direct memory mapping (if needed for bulk operations)
	virtual void* get_mmio_base() = 0;
	virtual int get_mmio_size() const = 0;
	virtual unsigned int get_cpu_offset() const = 0;
};

// Factory function to create platform-specific implementation
MMIOInterface* create_mmio_interface();

// Global accessor (singleton pattern)
MMIOInterface* get_mmio_interface();
void destroy_mmio_interface();

#endif // _MMIO_INTERFACE_H
