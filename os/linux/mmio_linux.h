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

#ifndef _MMIO_LINUX_H
#define _MMIO_LINUX_H

#include "mmio_interface.h"
#include <pciaccess.h>
#include <mutex>

/**
 * @brief Linux implementation of MMIO interface using libpciaccess
 */
class MMIOLinux : public MMIOInterface {
public:
	MMIOLinux();
	~MMIOLinux() override;

	int initialize(const char* device_str) override;
	int cleanup() override;
	bool is_initialized() const override;
	int get_device_id(const char* device_str) override;

	uint32_t read_reg(uint64_t offset) override;
	void write_reg(uint64_t offset, uint32_t value) override;

	void* get_mmio_base() override;
	int get_mmio_size() const override;
	unsigned int get_cpu_offset() const override;

private:
	struct pci_device* intel_get_pci_device(const char* device_str);
	int intel_mmio_use_pci_bar(struct pci_device* pci_dev);

	struct pci_device* pci_dev_;
	unsigned char* mmio_base_;
	int mmio_size_;
	int fd_;
	unsigned int cpu_offset_;
	bool initialized_;
	std::mutex mutex_;

	static constexpr int MMIO_SIZE = 2 * 1024 * 1024;
	static constexpr int MMIO_BAR = 0;
};

#endif // _MMIO_LINUX_H
