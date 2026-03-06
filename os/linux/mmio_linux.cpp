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

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <cerrno>
#include <limits.h>
#include "mmio_linux.h"
#include <debug.h>

MMIOLinux::MMIOLinux()
	: pci_dev_(nullptr)
	, mmio_base_(nullptr)
	, mmio_size_(0)
	, fd_(-1)
	, cpu_offset_(0)
	, initialized_(false)
{
}

MMIOLinux::~MMIOLinux() {
	cleanup();
}

/**
* @brief
* Looks up the main graphics pci device using libpciaccess.
* @param device_str - The device string to look up
* @return The pci device or nullptr in case of a failure
*/
struct pci_device* MMIOLinux::intel_get_pci_device(const char *device_str)
{
	struct pci_device *pci_dev;
	int error;
	char sysfs_device_path[PATH_MAX];
	char resolved_path[PATH_MAX];
	unsigned int domain, bus, dev, func;

	if (!device_str) {
		ERR("Device string is nullptr\n");
		return nullptr;
	}

	// Construct the sysfs device path
	snprintf(sysfs_device_path, sizeof(sysfs_device_path), "/sys/class/drm/%s/device", strrchr(device_str, '/') + 1);

	// Resolve the symlink to something like "/sys/devices/pci0000:00/0000:00:02.0"
	if (realpath(sysfs_device_path, resolved_path) == nullptr) {
		ERR("Failed to resolve device symlink");
		return nullptr;
	}

	// Parse the PCI domain:bus:device.function (e.g., "0000:00:02.0")
	const char* pci_info = strrchr(resolved_path, '/');
	if (!pci_info) {
		ERR("Invalid resolved path: %s\n", resolved_path);
		return nullptr;
	}

	pci_info++; // Skip the '/' to get to the actual "0000:00:02.0"

	if (sscanf(pci_info, "%x:%x:%x.%x", &domain, &bus, &dev, &func) != 4) {
		ERR("Failed to parse PCI info from '%s'\n", pci_info);
		return nullptr;
	}

	error = pci_system_init();
	if(error) {
		ERR("Couldn't initialize PCI system\n");
		return nullptr;
	}

	// Find the PCI device using the parsed domain, bus, device, and function
	pci_dev = pci_device_find_by_slot(domain, bus, dev, func);
	if (pci_dev == nullptr || pci_dev->vendor_id != 0x8086) {
		struct pci_device_iterator *iter;
		struct pci_id_match match;

		match.vendor_id = 0x8086; // Intel
		match.device_id = PCI_MATCH_ANY;
		match.subvendor_id = PCI_MATCH_ANY;
		match.subdevice_id = PCI_MATCH_ANY;

		match.device_class = 0x3 << 16;
		match.device_class_mask = 0xff << 16;

		match.match_data = 0;

		iter = pci_id_match_iterator_create(&match);
		pci_dev = pci_device_next(iter);
		pci_iterator_destroy(iter);
	}

	if(!pci_dev) {
		ERR("Couldn't find Intel graphics card\n");
		return nullptr;
	}

	error = pci_device_probe(pci_dev);
	if(error) {
		ERR("Couldn't probe graphics card\n");
		return nullptr;
	}

	if (pci_dev->vendor_id != 0x8086) {
		ERR("Graphics card is non-intel\n");
		return nullptr;
	}

	return pci_dev;
}

/**
* @brief
* Fill a mmio_data stucture with igt_mmio to point at the mmio bar.
* @param *pci_dev - intel graphics pci device
* @return
* - 0 = SUCCESS
* - 1 = FAILURE
*/
int MMIOLinux::intel_mmio_use_pci_bar(struct pci_device *pci_dev)
{
	int mmio_bar, mmio_size;
	int error;

	mmio_bar = 0;
	mmio_size = MMIO_SIZE;

	error = pci_device_map_range(pci_dev,
					pci_dev->regions[mmio_bar].base_addr,
					mmio_size,
					PCI_DEV_MAP_FLAG_WRITABLE,
					(void **) &mmio_base_);

	if(error) {
		ERR("Couldn't map MMIO region\n");
		return 1;
	}

	mmio_size_ = mmio_size;
	return 0;
}

int MMIOLinux::initialize(const char* device_str) {
	std::lock_guard<std::mutex> lock(mutex_);

	if (initialized_) {
		return 0; // Already initialized
	}

	pci_dev_ = intel_get_pci_device(device_str);
	if (!pci_dev_) {
		return 1;
	}

	if (intel_mmio_use_pci_bar(pci_dev_)) {
		pci_dev_ = nullptr;
		return 1;
	}

	initialized_ = true;
	return 0;
}

int MMIOLinux::cleanup() {
	std::lock_guard<std::mutex> lock(mutex_);

	int status = 0;

	if (pci_dev_ && mmio_base_) {
		if (pci_device_unmap_range(pci_dev_, mmio_base_, MMIO_SIZE) != 0) {
			ERR("Failed to unmap MMIO range.\n");
			status = 1;
		}
		mmio_base_ = nullptr;
	}

	pci_dev_ = nullptr;

	pci_system_cleanup(); // no error return, assumed to succeed

	if (fd_ >= 0) {
		if (close(fd_) == -1) {
			ERR("Failed to properly close file descriptor. Error: %s\n", strerror(errno));
			status = 1;
		}
		fd_ = -1;
	}

	initialized_ = false;
	return status;
}

bool MMIOLinux::is_initialized() const {
	return initialized_;
}

int MMIOLinux::get_device_id(const char* device_str) {
	std::lock_guard<std::mutex> lock(mutex_);

	if (!pci_dev_) {
		pci_dev_ = intel_get_pci_device(device_str);
	}
	return pci_dev_ ? pci_dev_->device_id : 0;
}

uint32_t MMIOLinux::read_reg(uint64_t offset) {
	if (!initialized_ || !mmio_base_) {
		ERR("MMIO not initialized\n");
		return 0;
	}
	return *((volatile uint32_t*)(mmio_base_ + offset + cpu_offset_));
}

void MMIOLinux::write_reg(uint64_t offset, uint32_t value) {
	if (!initialized_ || !mmio_base_) {
		ERR("MMIO not initialized\n");
		return;
	}
	*((volatile uint32_t*)(mmio_base_ + offset + cpu_offset_)) = value;
}

void* MMIOLinux::get_mmio_base() {
	return mmio_base_;
}

int MMIOLinux::get_mmio_size() const {
	return mmio_size_;
}

unsigned int MMIOLinux::get_cpu_offset() const {
	return cpu_offset_;
}
