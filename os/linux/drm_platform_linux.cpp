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

#include "drm_platform.h"
#include "debug.h"
#include "mmio.h"
#include "os_macros.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

// Hammock Harbor registers (from vsyncalter.cpp)
#define HH_LIVE_TS_REG_HIGH 0x462f8
#define HH_LIVE_TS_REG_LOW 0x462f4
constexpr long double HH_CLOCK_FREQUENCY_LOCAL = 38.4L;

typedef struct {
	uint32_t low;
	uint32_t high;
} hh_timestamp_regs_t;

static const hh_timestamp_regs_t pipe_timestamp_regs[4] = {
	{ 0x46300, 0x46304 }, /* Pipe 0 */
	{ 0x46308, 0x4630C }, /* Pipe 1 */
	{ 0x46310, 0x46314 }, /* Pipe 2 */
	{ 0x46318, 0x4631C }  /* Pipe 3 */
};

/**
 * @brief VBlank event handler callback
 *        Called by DRM when a vblank event occurs
 */
static void vblank_handler(int fd, unsigned int frame UNUSED, unsigned int sec,
			   unsigned int usec, void *data)
{
	drmVBlank vbl;
	os_vbl_info *info = (os_vbl_info *)data;
	memset(&vbl, 0, sizeof(drmVBlank));
	uint64_t timestamp = 0;

	if(info->counter < info->size) {
		if (info->hardware_ts) {
			// Hardware timestamping mode: Use Hammock Harbor (HH) timestamp registers
			struct timespec ts_realtime;
			const hh_timestamp_regs_t *regs = &pipe_timestamp_regs[info->pipe];

			timestamp = (((uint64_t)READ_OFFSET_DWORD(regs->high)) << 32) | READ_OFFSET_DWORD(regs->low);
			uint64_t timestamp_live = (((uint64_t)READ_OFFSET_DWORD(HH_LIVE_TS_REG_HIGH)) << 32) | READ_OFFSET_DWORD(HH_LIVE_TS_REG_LOW);

			clock_gettime(CLOCK_REALTIME, &ts_realtime);

			// Convert CLOCK_REALTIME to µs
			uint64_t realtime_us = (uint64_t)ts_realtime.tv_sec * 1000000ULL +
								(uint64_t)(ts_realtime.tv_nsec / 1000ULL);

			// Delta in HH ticks since last vblank happened.
			uint64_t timestamp_diff = timestamp_live - timestamp;

			/* Scale from 38.4MHz to us */
			timestamp_diff =
				(uint64_t)(((long double)timestamp_diff) / HH_CLOCK_FREQUENCY_LOCAL);

			// Calculate vblank timestamp by subtracting elapsed time from realtime
			timestamp = realtime_us - timestamp_diff;
		} else {
			timestamp = TIME_IN_USEC(sec, usec);
		}

		info->vsync_array[info->counter++] = timestamp;
	}

	vbl.request.type = (drmVBlankSeqType) (DRM_VBLANK_RELATIVE | DRM_VBLANK_EVENT |
		os_pipe_to_wait_for(info->pipe));
	vbl.request.sequence = 1;
	vbl.request.signal = (unsigned long)data;

	drmWaitVBlank(fd, &vbl);
}

int os_print_drm_info(const char *device_str)
{
	// This list covers most of the connector types that are supported by the DRM
	const char* connector_type_str[] = {
		"Unknown",      // 0
		"VGA",          // 1
		"DVI-I",        // 2
		"DVI-D",        // 3
		"DVI-A",        // 4
		"Composite",    // 5
		"S-Video",      // 6
		"LVDS",         // 7
		"Component",    // 8
		"9PinDIN",      // 9
		"DisplayPort",  // 10
		"HDMI-A",       // 11
		"HDMI-B",       // 12
		"TV",           // 13
		"eDP",          // 14
		"Virtual",      // 15
		"DSI",          // 16
		"DPI",          // 17
		"WriteBack",    // 18
		"SPI",          // 19
		"USB"           // 20
	};

	int fd = open(device_str, O_RDWR | O_CLOEXEC, 0);
	if (fd < 0) {
		ERR("Failed to open DRM device: %s (%s)\n", device_str, strerror(errno));
		return 1;
	}

	drmModeRes *resources = drmModeGetResources(fd);
	if (!resources) {
		ERR("drmModeGetResources failed: %s\n", strerror(errno));
		if(close(fd) == -1){
			ERR("Failed to properly close file descriptor. Error: %s\n", strerror(errno));
		}
		return 1;
	}

	INFO("DRM Info:\n");

	// First print Pipe/CRTC info
	INFO("  CRTCs found: %d\n", resources->count_crtcs);
	for (int i = 0; i < resources->count_crtcs; i++) {
		drmModeCrtc *crtc = drmModeGetCrtc(fd, resources->crtcs[i]);
		if (!crtc) {
			ERR("drmModeGetCrtc failed: %s\n", strerror(errno));
			continue;
		}

		double refresh_rate = 0;
		if (crtc->mode_valid) {
			refresh_rate = static_cast<double>(crtc->mode.clock) * 1000.0 / (crtc->mode.vtotal * crtc->mode.htotal);
		}

		INFO("  \tPipe: %2d, CRTC ID: %4d, Mode Valid: %3s, Mode Name: %s, Position: (%4d, %4d), Resolution: %4dx%-4d, Refresh Rate: %.2f Hz\n",
			   i, crtc->crtc_id, (crtc->mode_valid) ? "Yes" : "No", crtc->mode.name,
				crtc->x, crtc->y, crtc->mode.hdisplay, crtc->mode.vdisplay, refresh_rate);

		drmModeFreeCrtc(crtc);
	}

	// Print Connector info
	INFO("  Connectors found: %d\n", resources->count_connectors);
	for (int i = 0; i < resources->count_connectors; i++) {
		drmModeConnector *connector = drmModeGetConnector(fd, resources->connectors[i]);
		if (!connector) {
			ERR("drmModeGetConnector failed: %s\n", strerror(errno));
			continue;
		}

		const char* type_name = (connector->connector_type < ARRAY_SIZE(connector_type_str))
				? connector_type_str[connector->connector_type]
				: "Unknown";

		INFO("  \tConnector: %-4d (ID: %-4d), Type: %-4d (%-12s), Type ID: %-4d, Connection: %-12s\n",
				i, connector->connector_id, connector->connector_type, type_name,
				connector->connector_type_id,
				(connector->connection == DRM_MODE_CONNECTED) ? "Connected" : "Disconnected");

		if (connector->connection == DRM_MODE_CONNECTED) {
			drmModeEncoder *encoder = drmModeGetEncoder(fd, connector->encoder_id);
			if (encoder) {
				INFO("\t\t\tEncoder ID: %d, CRTC ID: %d\n", encoder->encoder_id, encoder->crtc_id);
				drmModeFreeEncoder(encoder);
			}
		}
		drmModeFreeConnector(connector);
	}

	drmModeFreeResources(resources);
	if(close(fd) == -1){
		ERR("Failed to properly close file descriptor. Error: %s\n", strerror(errno));
		return 1;
	}

	return 0;
}

unsigned int os_pipe_to_wait_for(int pipe)
{
	int ret = 0;
	if (pipe > 1) {
		ret = (pipe << DRM_VBLANK_HIGH_CRTC_SHIFT) & DRM_VBLANK_HIGH_CRTC_MASK;
	} else if (pipe > 0) {
		ret = DRM_VBLANK_SECONDARY;
	}
	return ret;
}

int os_get_vsync(const char *device_str, uint64_t *vsync_array, int size, int pipe, bool hardware_ts)
{
	drmVBlank vbl;
	int ret;
	drmEventContext evctx;
	os_vbl_info handler_info;

	// Validate parameters
	if (device_str == nullptr || strlen(device_str) == 0) {
		ERR("Invalid device string (nullptr or empty)\n");
		return 1;
	}

	if (vsync_array == nullptr) {
		ERR("nullptr vsync_array pointer provided\n");
		return 1;
	}

	if (size <= 0) {
		ERR("Invalid size (must be > 0): %d\n", size);
		return 1;
	}

	int fd = open(device_str, O_RDWR | O_CLOEXEC, 0);
	if(fd < 0) {
		ERR("Couldn't open %s. Is i915 installed?\n", device_str);
		return 1;
	}

	memset(&vbl, 0, sizeof(drmVBlank));

	handler_info.vsync_array = vsync_array;
	handler_info.size = size;
	handler_info.counter = 0;
	handler_info.pipe = pipe;
	handler_info.hardware_ts = hardware_ts;

	// Queue an event for frame + 1
	vbl.request.type = (drmVBlankSeqType)
		(DRM_VBLANK_RELATIVE | DRM_VBLANK_EVENT | os_pipe_to_wait_for(pipe));
	DBG("vbl.request.type = 0x%X\n", vbl.request.type);
	vbl.request.sequence = 1;
	vbl.request.signal = (unsigned long)&handler_info;
	ret = drmWaitVBlank(fd, &vbl);
	if (ret) {
		ERR("drmWaitVBlank (relative, event) failed ret: %i\n", ret);
		close(fd);
		return 1;
	}

	// Set up our event handler
	memset(&evctx, 0, sizeof evctx);
	evctx.version = DRM_EVENT_CONTEXT_VERSION;
	evctx.vblank_handler = vblank_handler;
	evctx.page_flip_handler = nullptr;

	// Poll for events
	for(int i = 0; i < size; i++) {
		struct timeval timeout = { .tv_sec = 3, .tv_usec = 0 };
		fd_set fds;

		FD_ZERO(&fds);
		FD_SET(0, &fds);
		FD_SET(fd, &fds);
		ret = select(fd + 1, &fds, nullptr, nullptr, &timeout);

		if (ret <= 0) {
			ERR("select timed out or error (ret %d)\n", ret);
			continue;
		}

		ret = drmHandleEvent(fd, &evctx);
		if (ret) {
			ERR("drmHandleEvent failed: %i\n", ret);
			close(fd);
			return 1;
		}
	}

	close(fd);
	return 0;
}
