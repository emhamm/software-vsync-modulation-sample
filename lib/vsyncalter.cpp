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

// C++ standard headers (alphabetically)
#include <cassert>
#include <cerrno>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <list>
#include <string>

using std::list;
using std::string;

// Project headers
#include <vsyncalter.h>

// Platform abstraction
#include "file_platform.h"
#include "drm_platform.h"
#include "system_platform.h"
#include <debug.h>
#include <adl_p.h>
#include <adl_s.h>
#include <mtl.h>
#include <ptl.h>
#include <tgl.h>

#include "c10.h"
#include "c20.h"
#include "combo.h"
#include "common.h"
#include "dkl.h"
#include "dp_m_n.h"
#include "i915_pciids.h"
#include "mmio.h"

const platform platform_table[] = {
	{"TGL",       {INTEL_TGL_IDS},       tgl_ddi_sel,   ARRAY_SIZE(tgl_ddi_sel),   4},
	{"ADL_S_FAM", {INTEL_ADL_S_FAM_IDS}, adl_s_ddi_sel, ARRAY_SIZE(adl_s_ddi_sel), 0},
	{"ADL_P_FAM", {INTEL_ADL_P_FAM_IDS}, adl_p_ddi_sel, ARRAY_SIZE(adl_p_ddi_sel), 4},
	{"MTL",       {INTEL_MTL_FAM_IDS},   mtl_ddi_sel,   ARRAY_SIZE(mtl_ddi_sel),   0},
	{"BMG",       {INTEL_BMG_FAM_IDS},   mtl_ddi_sel,   ARRAY_SIZE(mtl_ddi_sel),   0}, // BMG uses MTL PHY programming
	{"PTL",       {INTEL_PTL_FAM_IDS},   ptl_ddi_sel,   ARRAY_SIZE(ptl_ddi_sel),   0},
};

enum DisplayPortMode {
	DP_SST = 0x2,
	DP_MST = 0x3
};

constexpr long double HH_CLOCK_FREQUENCY = 38.4L;

int supported_platform = 0;
static list<phys *> *phy_enabled_list = nullptr;
std::atomic<bool> lib_client_done{false};
bool hardware_ts = false;

/* Timestamp register offsets for each pipe */
typedef struct {
	uint32_t low;  /* Lower 32-bit register offset */
	uint32_t high; /* Upper 32-bit register offset */
} hh_timestamp_regs_t;

// Panther Lake Hammock Harbor timestamping registers
static const hh_timestamp_regs_t pipe_timestamp_regs[4] = {
	{ 0x46300, 0x46304 }, /* Pipe 0 */
	{ 0x46308, 0x4630C }, /* Pipe 1 */
	{ 0x46310, 0x46314 }, /* Pipe 2 */
	{ 0x46318, 0x4631C }  /* Pipe 3 */
};

#define HH_ENABLE_REG 0x462FC
#define HH_LIVE_TS_REG_HIGH 0x462f8
#define HH_LIVE_TS_REG_LOW 0x462f4
#define HH_ENABLE_BIT 31

/**
* @brief
*	This function creates a timer.
* @param  expire_ms - The time period in ms after which the timer will fire.
* @param *user_ptr - A pointer to pass to the timer handler
* @param *t - A pointer to a pointer where we need to store the timer
* @return
* - 0 == SUCCESS
* - 1 = FAILURE
*/
int phys::make_timer(long expire_ms, void *user_ptr, reset_func reset)
{
	return os_make_timer(expire_ms, user_ptr, reset, (void**)&timer_id);
}

/**
* @brief
* This function opens a device.  e.g /dev/dri/card0
* @param *device_str - A string pointer with device identifier
* @return
* - 0 == SUCCESS
* - <0 == FAILURE
*/
int open_device(const char *device_str)
{
	if (!device_str) {
		ERR("Device string is NULL\n");
		return -1;
	}
	return os_open_device(device_str);
}

/**
* @brief
* This function closes an open handle
* @param fd - file handle to close
* @return void
*/
int close_device(int fd)
{
	os_close_device(fd);
	return 0;
}

/**
 * @brief
 * This function finds enabled PHYs on a system
 *
 * @param m_n - Whether to use DP M & N mode
 * @return int - 0 success , 1 failure
 */
int find_enabled_phys(bool m_n)
{
	int i, j, val, ddi_select;
	reg trans_ddi_func_ctl[] = {
		REG(TRANS_DDI_FUNC_CTL_A),
		REG(TRANS_DDI_FUNC_CTL_B),
		REG(TRANS_DDI_FUNC_CTL_C),
		REG(TRANS_DDI_FUNC_CTL_D),
	};

	// According to the BSpec:
	// 0000b	None
	// 0001b	DDI A
	// 0010b	DDI B
	// 0011b	DDI C
	// 0100b	DDI USBC1
	// 0101b	DDI USBC2
	// 0110b	DDI USBC3
	// 0111b	DDI USBC4
	// 1000b	DDI USBC5
	// 1001b	DDI USBC6
	// 1000b	DDI D
	// 1001b	DDI E
	// DISPLAY_CCU / DPCLKA Clock	DE Internal Clock
	// DDIA_DE_CLK	                DDIA
	// DDIB_DE_CLK	                USBC1
	// DDII_DE_CLK	                USBC2
	// DDIJ_DE_CLK	                USBC3
	// DDIK_DE_CLK	                USBC4

	phy_enabled_list = new list<phys *>;

	for(i = 0; i < ARRAY_SIZE(trans_ddi_func_ctl); i++) {
		// First read the TRANS_DDI_FUNC_CTL to find if this pipe is enabled or not
		val = READ_OFFSET_DWORD(trans_ddi_func_ctl[i].addr);
		DBG("0x%lX = 0x%X\n", trans_ddi_func_ctl[i].addr, val);
		if(!(val & BIT(31))) {
			DBG("Pipe %d is turned off\n", i); // 0 based index for pipe # printing
			continue;
		}

		DBG("Pipe %d is turned on\n", i);

		// TRANS_DDI_FUNC_CTL bits 26:24 tells us if it's a DP panel
		uint32_t mode_select = GETBITS_VAL(val, 26, 24);

		// TRANS_DDI_FUNC_CTL bits 30:27 have the DDI which this pipe is connected to
		ddi_select = GETBITS_VAL(val, 30, 27);
		DBG("ddi_select = 0x%X\n", ddi_select);
		for(j = 0; j < platform_table[supported_platform].ds_size; j++) {

			phys *new_phy = nullptr;
			// Match the DDI with the available ones on this platform
			if(platform_table[supported_platform].ds[j].de_clk == ddi_select) {
				if (m_n && (mode_select == DP_SST || mode_select == DP_MST )) {
					DBG("Found DP Panel on pipe %d.  Using M & N Path\n", i);
					new_phy = new dp_m_n(&platform_table[supported_platform].ds[j], i);
				}
				else {
					switch(platform_table[supported_platform].ds[j].phy) {
						case DKL:
							DBG("Detected a DKL phy on pipe %d\n", i);
							new_phy = new dkl(&platform_table[supported_platform].ds[j],
								platform_table[supported_platform].first_dkl_phy_loc, i);
							break;
						case COMBO:
							DBG("Detected a Combo phy on pipe %d\n", i);
								new_phy = new combo(&platform_table[supported_platform].ds[j], i);
								break;
						case C10:
							DBG("Detected a C10 phy on pipe %d\n", i);
							new_phy = new c10(&platform_table[supported_platform].ds[j], i);
							break;
						case C20:
							DBG("Detected a C20 phy on pipe %d\n", i);
							new_phy = new c20(&platform_table[supported_platform].ds[j], i,
								platform_table[supported_platform].name == string("BMG")); // BMG uses C20 PHY
							break;
						default:
							ERR("Unsupported PHY. Phy is %d\n", platform_table[supported_platform].ds[j].phy);
							return 1;
							break;
					}
				}
				// Ignore PHYs that we don't support yet
				if (new_phy == nullptr) {
					continue;
				}
				if(!new_phy->is_init()) {
					ERR("PHY not initialized properly\n");
					delete new_phy;
					return 1;
				}

				phy_enabled_list->push_back(new_phy);

				// No point trying to find the same ddi if we have already found it once
				break;
			}
		}
	}
	return 0;
}

/**
* @brief
* This function deallocates all members of the phy_enabled_list
* @param None
* @return
* - 0 == SUCCESS
* - 1 == FAILURE
*/

int cleanup_phy_list()
{
	if (!phy_enabled_list) {
		return 0; // Nothing to clean up
	}

	for (std::list<phys *>::iterator it = phy_enabled_list->begin();
		 it != phy_enabled_list->end(); ++it) {
		if (*it) {
			delete *it;
		}
	}

	delete phy_enabled_list;
	phy_enabled_list = nullptr;

	return 0; // Success
}

/**
* @brief
* Sets a global flag to indicate client app termination via Ctrl+C
* @param None
* @return void
*/
void shutdown_lib(void)
{
	lib_client_done = 1;
}

/**
* @brief
* Prints the DRM information.	Loops through all CRTCs and Connectors and prints
* their information.
* @param *device_str - A string pointer containing device identifier
* @return
* - 0 == SUCCESS
* - 1 == FAILURE
*/
int print_drm_info(const char *device_str)
{
	return os_print_drm_info(device_str);
}

/**
* @brief
* This function initializes the library. It must be called
* ahead of all other functions because it opens device, maps MMIO space and
* initializes any key global variables.
* @param device_str - The device string identifier (e.g., "/dev/dri/card0")
* @param dp_m_n - Whether to use DisplayPort M & N mode for timing control
* @param h_ts - Whether to enable hardware timestamping (Hammock Harbor feature)
* @return
* - 0 == SUCCESS
* - 1 == FAILURE
*/
int vsync_lib_init(const char *device_str, bool dp_m_n, bool h_ts)
{
	int device_id, i, j;
	if(!IS_INIT()) {

		if (!device_str) {
			ERR("Device string is nullptr\n");
			return 1;
		}

		// Check if device string is valid
		int fd = open_device(device_str);
		if (fd < 0) {
			ERR("Failed to open DRM device: %s (%s)\n", device_str, strerror(errno));
			return 1;
		}
		close_device(fd);

		// Get the device id of this platform
		device_id = get_device_id(device_str);
		DBG("Device id is 0x%X\n", device_id);
		// Loop through our supported platform list
		for(i = 0; i < ARRAY_SIZE(platform_table); i++) {
			for(j = 0; j < MAX_DEVICE_ID; j++) {
				if(platform_table[i].device_ids[j] == device_id) {
					break;
				}
			}
			if(j != MAX_DEVICE_ID) {
				break;
			}
		}
		// This means we aren't on one of the supported platforms
		if(i == ARRAY_SIZE(platform_table)) {
			ERR("This platform is not supported. Device id is 0x%X\n", device_id);
			return 1;
		} else {
			supported_platform = i;
		}

		if(map_mmio(device_str)) {
			return 1;
		}

		if(find_enabled_phys(dp_m_n)) {
			vsync_lib_uninit();
			return 1;
		}

		if (h_ts) {
			hardware_ts = h_ts;
			uint32_t val = READ_OFFSET_DWORD(HH_ENABLE_REG);
			SETBIT_INPLACE(val, HH_ENABLE_BIT); // Enable HH
			WRITE_OFFSET_DWORD(HH_ENABLE_REG, val); // Enable HH
		}

		INIT();
	}

	return 0;
}

/**
* @brief
* This function uninitializes the library by closing devices
* and unmapping memory. It must be called at program exit or else we can have
* memory leaks in the program.
* @param None
* @return
* - 0 == SUCCESS
* - 1 == FAILURE
*/
int vsync_lib_uninit()
{
	int status = 0;

	if (cleanup_phy_list() != 0) {
		ERR("Failed to clean up PHY list.\n");
		status = 1;
	}

	if (close_mmio_handle() != 0) {
		ERR("Failed to close MMIO handle.\n");
		status = 1;
	}

	UNINIT(); // no return value assumed

	return status;
}

/**
* @brief
* This function synchronizes the primary and secondary
* systems vsync. It is run on the secondary system. The way that it works is
* that it finds out the default PHY register values, calculates a shift
* based on the time difference provided by the caller and then reprograms the
* PHY registers so that the secondary system can either slow down or speed up
* its vsnyc durations. Drifts is applied as percentage shift to the original `pll_clock`.
* The direction of adjustment is determined by `time_diff`:
*	- A **positive `time_diff`** indicates that timestamps should drift forward, requiring a **decrease** in PLL frequency.
*	  This increases the vblank time period, effectively delaying the timestamps.
*	- A **negative `time_diff`** indicates that timestamps should drift backward, requiring an **increase** in PLL frequency.
*	  This decreases the vblank time period, effectively advancing the timestamps.
* @param time_diff - This is the time difference in between the primary and the
* secondary systems in ms. If primary is ahead of the secondary , then the time
* difference is a positive number otherwise negative.
* @param pipe - This is the 0 based pipe number to synchronize vsyncs for. Note
* that this variable is optional. So if the caller doesn't provide it or provides
* ALL_PIPES as the value, then all pipes will be synchronized.
* @param shift - This is the amount of shift that we need to apply
* to the secondary system's vsync period. This is a positive floating point number
* and to be considered as change fraction (e.g .01).
* @param shift2 - Shift value for stepping mode
* @param step_threshold - step_threshold  Delta threshold in microseconds to trigger stepping mode
* @param wait_between_steps - Wait in milliseconds between steps
* @param reset - This is a boolean value. If true, then we will reset the
* registers to their original values after the shift has been applied.
* @param commit - This is a boolean value. If false, then we will not program
* the PHY registers. This is useful for debugging purposes.
* @return
* - 0 == SUCCESS
* - 1 == FAILURE
*/
int synchronize_vsync(double time_diff, int pipe, double shift, double shift2, int step_threshold,
						int wait_between_steps, bool reset, bool commit)
{
	if (!IS_INIT()) {
		ERR("Uninitialized lib, please call lib_init() first.\n");
		return 1;
	}
	if (fabs(time_diff) >= 20 || shift < 0.0 || shift2 < 0.0 || shift > 1.0 || shift2 > 1.0)
		return 1;

	if (!phy_enabled_list) {
		ERR("PHY list not initialized.\n");
		return 1;
	}

	bool matched = false;
	int status = 0;

	for (std::list<phys *>::iterator it = phy_enabled_list->begin();
		it != phy_enabled_list->end(); ++it) {

		if (!*it) continue;

		if (pipe == VSYNC_ALL_PIPES || pipe == (*it)->get_pipe()) {
			matched = true;

			if ((*it)->program_phy(time_diff, shift, shift2, step_threshold, wait_between_steps, reset, commit) != 0) {
				ERR("Failed to program PHY on pipe %d.\n", (*it)->get_pipe());
				status = 1;
				continue;  // continue trying other PHYs
			}

			if (reset && commit) {
				(*it)->wait_until_done();
			}
		}
	}

	if (!matched) {
		ERR("No matching PHY found for pipe %d.\n", pipe);
		return 1;
	}

	return status;
}

/**
* @brief
* This function determines the type of vblank synchronization to
* use for the output.
* @param pipe - Indicates which CRTC to get vblank for.  Knowing this, we
* can determine which vblank sequence type to use for it.  Traditional
* cards had only two CRTCs, with CRTC 0 using no special flags, and
* CRTC 1 using DRM_VBLANK_SECONDARY.  The first bit of the pipe
* Bits 1-5 of the pipe parameter are 5 bit wide pipe number between
* 0-31.  If this is non-zero it indicates we're dealing with a
* multi-gpu situation and we need to calculate the vblank sync
* using DRM_BLANK_HIGH_CRTC_MASK.
* @return The flag to OR in for drmWaitVBlank API
*/
unsigned int pipe_to_wait_for(int pipe)
{
	return os_pipe_to_wait_for(pipe);
}

/**
* @brief
* This function gets a list of vsyncs (as microseconds) for the number of times
* indicated by the caller and provide their timestamps in the array provided.
* Timestamps are returned in chronological order. [0] -> oldest and [size - 1] -> latest.
* @param device_str - The device string identifier (e.g., "/dev/dri/card0")
* @param *vsync_array - The array in which vsync timestamps need to be given
* @param size - The size of this array. This is also the number of times that we
* need to get the next few vsync timestamps.
* @param pipe - This is the pipe whose vblank is needed. Defaults to 0 if not
* provided.
* @return
* - 0 == SUCCESS
* - 1 = ERROR
*/
int get_vsync(const char *device_str, uint64_t *vsync_array, int size, int pipe)
{
	// Validate max size constraint specific to this API
	if (size > VSYNC_MAX_TIMESTAMPS) {
		ERR("Requested size (%d) exceeds VSYNC_MAX_TIMESTAMPS (%d)\n", size, VSYNC_MAX_TIMESTAMPS);
		return 1;
	}

	return os_get_vsync(device_str, vsync_array, size, pipe, hardware_ts);
}


/**
 * @brief
 * This function collects vblank timestamps and prints average interval between them
 *
 * @param device_str - The device string, e.g. "/dev/dri/card0"
 * @param pipe - The pipe number
 * @param size - The number of timestamps to collect
 * @return double - The average vblank interval in milliseconds, or 0.0 on error
 */
double get_vblank_interval(const char *device_str, int pipe, int size)
{

	uint64_t timestamps[VSYNC_MAX_TIMESTAMPS];  // Allocate enough buffer

	if (size > VSYNC_MAX_TIMESTAMPS) {
		ERR("Requested size exceeding maximum\n");
		return 0.0;
	}

	if (size < 2) {
		ERR("Requested size is not sufficient: size=%d\n", size);
		return 0.0;
	}

	if (get_vsync(device_str, timestamps, size, pipe) == 0) {
			int64_t total_interval = 0;
			for (int i = 0; i < size - 1; ++i) {
					total_interval += (timestamps[i+1] - timestamps[i]);
			}

			double avg_interval =  ((double)total_interval) / (size - 1) / 1000.0; // Convert to milliseconds
			return avg_interval;
	}
	return 0.0;
}

/**
 * @brief
 * This function sets the PLL clock for the given pipe
 * @param pll_clock - The desired PLL clock
 * @param pipe - The pipe to set the PLL clock for
 * @param shift - Fraction value to be used during the calculation.
 * @param wait_between_steps - Wait in milliseconds to be applied between steps after
 * each time programming the registers.
* @return
* - 0 == SUCCESS
* - 1 == FAILURE
 */
int set_pll_clock(double pll_clock, int pipe, double shift, uint32_t wait_between_steps)
{
	if(!IS_INIT()) {
		ERR("Uninitialized lib, please call lib init first\n");
		return 1;
	}

	if (!phy_enabled_list) {
		ERR("PHY list is not initialized\n");
		return 1;
	}

	int result = 0;

	for(list<phys *>::iterator it = phy_enabled_list->begin();
		it != phy_enabled_list->end(); it++) {
			if(pipe == VSYNC_ALL_PIPES || pipe == (*it)->get_pipe()) {
				// Set pll clock for this pipe
				int ret = (*it)->set_pll_clock(pll_clock, shift, wait_between_steps);
				if (ret != 0) {
					ERR("Failed to set PLL clock for pipe %d\n", (*it)->get_pipe());
					result = ret;  // Continue attempting others, but capture the error
				}
			}
	}

	return result;
}

/**
 * @brief
 * This function returns the PLL clock for the given pipe
 * @param pipe - The pipe to get the PLL clock for
 * @return double - The PLL clock for the given pipe
 */
double get_pll_clock(int pipe)
{
	if(!IS_INIT()) {
		ERR("Uninitialized lib, please call lib init first\n");
		return 0.0;
	}
	if (pipe == VSYNC_ALL_PIPES) {
		ERR("Pipe not given\n");
		return 0.0;
	}

	if(phy_enabled_list) {
		for(list<phys *>::iterator it = phy_enabled_list->begin();
			it != phy_enabled_list->end(); it++) {
				if(pipe == (*it)->get_pipe()) {
					// Set pll clock for this pipe
					return (*it)->get_pll_clock();
				}
			}
	}

	return 0.0;
}

/**
* @brief
*	Utility function to return first available DRI card device path.
* @param none
* @return const char* - Path to the first available DRI card (e.g., "/dev/dri/card0")
*/
const char* find_first_dri_card(void) {
	static char path[32];
	#define MAX_CARDS 4
	#define DEVICE_PATH_FORMAT "/dev/dri/card%d"

	for (int i = 0; i < MAX_CARDS; i++) {
		snprintf(path, sizeof(path), DEVICE_PATH_FORMAT, i);
		if (os_stat_file(path) == 0) {
			return path;
		}
	}

	// No card found, fallback to default
	snprintf(path, sizeof(path), DEVICE_PATH_FORMAT, 0);
	return path;
}

/**
* @brief
*	Utility function to get the readable PHY type name for a given pipe.
* @param pipe - The pipe number to get PHY information for
* @param out_name - Output buffer to store the PHY name string
* @param out_size - Size of the output buffer
* @return bool - true if PHY name was successfully retrieved, false otherwise
*/
bool get_phy_name(int pipe, char* out_name, size_t out_size) {

	if (!out_name || out_size == 0) {
		ERR("Invalid output buffer.\n");
		return false;
	}

	// Initialize output buffer
	out_name[0] = '\0';

	if (!IS_INIT()) {
		ERR("Library uninitialized. Please call lib_init() first.\n");
		return false;
	}

	if (pipe == VSYNC_ALL_PIPES) {
		ERR("Invalid pipe: ALL_PIPES provided.\n");
		return false;
	}

	if (!phy_enabled_list) {
		return false;
	}

	bool found = false;
	for (const auto& phy : *phy_enabled_list) {
		if (phy && pipe == phy->get_pipe()) {
			switch (phy->get_phy_type()) {
				case DKL:
					snprintf(out_name, out_size, "Dekel");
					break;
				case COMBO:
					snprintf(out_name, out_size, "Combo");
					break;
				case M_N:
					snprintf(out_name, out_size, "M_N");
					break;
				case C10:
					snprintf(out_name, out_size, "C10");
					break;
				case C20:
					snprintf(out_name, out_size, "C20");
					break;
				default:
					snprintf(out_name, out_size, "Unknown");
					break;
			}
			found = true;
			break;  // Exit loop once match is found
		}
	}

	if (!found) {
		WARNING("No matching PHY found for pipe %d\n", pipe);
		return false;
	}

	return true;
}
