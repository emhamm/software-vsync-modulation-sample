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

#include "mmio_interface.h"

#ifdef __linux__
    #include "linux/mmio_linux.h"
#endif

static MMIOInterface* g_mmio_instance = nullptr;

/**
 * @brief Factory function to create platform-specific MMIO implementation
 * @return Pointer to platform-specific MMIOInterface implementation
 */
MMIOInterface* create_mmio_interface() {
#ifdef __linux__
    return new MMIOLinux();
#endif
}

/**
 * @brief Get the global MMIO interface instance (singleton)
 * @return Pointer to MMIOInterface instance
 */
MMIOInterface* get_mmio_interface() {
	if (!g_mmio_instance) {
		g_mmio_instance = create_mmio_interface();
	}
	return g_mmio_instance;
}

/**
 * @brief Destroy the global MMIO interface instance
 */
void destroy_mmio_interface() {
	if (g_mmio_instance) {
		delete g_mmio_instance;
		g_mmio_instance = nullptr;
	}
}
