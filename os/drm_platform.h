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

#ifndef _DRM_PLATFORM_H
#define _DRM_PLATFORM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Print DRM (Direct Rendering Manager) information for the given device.
 *        On Linux, this queries DRM for CRTCs and connectors.
 *        On Windows, this functionality is not available.
 *
 * @param device_str - Device identifier string (e.g., "/dev/dri/card0")
 * @return 0 on success, 1 on failure
 */
int os_print_drm_info(const char *device_str);

/**
 * @brief Internal structure to hold vblank timestamp information
 *        Used as callback context for vblank event handling
 */
typedef struct {
	uint64_t *vsync_array; ///< Array to store vblank timestamps
	int size;              ///< Size of the array
	int counter;           ///< Current counter/index
	int pipe;              ///< Pipe number
	bool hardware_ts;      ///< Whether to use hardware timestamping
} os_vbl_info;

/**
 * @brief Get vblank timestamps for the specified pipe
 *
 * @param device_str - Device identifier string
 * @param vsync_array - Array to store vblank timestamps (in microseconds)
 * @param size - Number of timestamps to collect
 * @param pipe - Pipe number to monitor
 * @param hardware_ts - Whether to use hardware timestamping (Hammock Harbor)
 * @return 0 on success, 1 on failure
 */
int os_get_vsync(const char *device_str, uint64_t *vsync_array, int size, int pipe, bool hardware_ts);

/**
 * @brief Calculate pipe flags for DRM vblank API
 *        Handles both legacy and multi-GPU configurations
 *
 * @param pipe - Pipe number
 * @return Flags to use with DRM vblank wait functions
 */
unsigned int os_pipe_to_wait_for(int pipe);

#ifdef __cplusplus
}
#endif

#endif // _DRM_PLATFORM_H
