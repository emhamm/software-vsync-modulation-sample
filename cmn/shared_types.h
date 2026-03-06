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

#ifndef _SHARED_TYPES_H
#define _SHARED_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
#include <cstdint>
#endif

// PHY types enumeration
enum {
	DKL,
	COMBO,
	M_N,
	C10,
	C20,
	TOTAL_PHYS,
};

// Register structure
typedef struct _reg {
	uint64_t addr;
	uint32_t orig_val;
	uint32_t mod_val;
} reg;

#ifdef __cplusplus
// User info class for PHY timer callbacks
class user_info {
private:
	void *phy_reg;
	void *phy_type;
public:
	user_info(void *t) { phy_type = t; }
	void *get_type() { return phy_type; }
	void *get_reg() { return phy_reg; }
};
#endif

// VBlank info structure
typedef struct _vbl_info {
	uint64_t *vsync_array;
	int size;
	int counter;
	int pipe;
} vbl_info;

// Signal handling function type - platform specific
#ifdef __linux__
#include <signal.h>
typedef void (*reset_func)(int sig, siginfo_t *si, void *uc);
#endif

// DDI selection structure
typedef struct _ddi_sel {
	char de_clk_name[20];
	int phy;
	int de_clk;
	reg dpclk;
	int clock_bit;
	int mux_select_low_bit;
	int dpll_num;
	void *phy_data;
} ddi_sel;

// Platform structure for Intel GPU generations
#define MAX_DEVICE_ID 40

typedef struct _platform {
	char name[20];
	int device_ids[MAX_DEVICE_ID];
	ddi_sel *ds;
	int ds_size;
	int first_dkl_phy_loc;
} platform;

#endif // _SHARED_TYPES_H
