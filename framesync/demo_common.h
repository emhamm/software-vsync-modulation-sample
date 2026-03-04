/*
 * Copyright © 2025-2026 Intel Corporation
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

#ifndef _DEMO_COMMON_H
#define _DEMO_COMMON_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <atomic>

#include "demo_protocol.h"
#include "vsyncalter.h"
#include "debug.h"

// Global shutdown flag
extern std::atomic<bool> g_shutdown;

// Configuration structure
struct DemoConfig {
	std::string mode;           // "primary" or "secondary"
	int pipe;                   // Display pipe/connector
	std::string device;         // DRM device (e.g., /dev/dri/card0)
	int interval_ms;            // Color change interval in milliseconds
	int schedule_ahead_ms;      // How many milliseconds ahead to schedule updates
	bool fullscreen;            // Fullscreen rendering
	int width;                  // Window width (if not fullscreen)
	int height;                 // Window height (if not fullscreen)
	bool vsync_enabled;         // Enable vsync (should be true for demo)
	bool cycle_colors;          // Auto-cycle through predefined colors
	std::string multicast_group; // UDP multicast group address for color updates
	int multicast_port;         // UDP multicast port
	int multicast_ttl;          // Multicast TTL (1=subnet, 32=campus, 255=unrestricted)
	std::string multicast_if;   // Multicast interface IP (empty = INADDR_ANY)

	DemoConfig() :
		mode("primary"),
		pipe(0),
		device(find_first_dri_card()),
		interval_ms(16),
		schedule_ahead_ms(16),
		fullscreen(false),
		width(1280),
		height(720),
		vsync_enabled(true),
		cycle_colors(true),
		multicast_group("239.1.1.1"),
		multicast_port(5001),
		multicast_ttl(1),
		multicast_if("")
	{}
};

// Forward declarations
int run_primary(const DemoConfig& config);
int run_secondary(const DemoConfig& config);

// Utility functions
void signal_handler(int sig);
void setup_signal_handlers();

// Convert color to RGBA components
inline void color_to_rgba(uint32_t color, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a) {
	r = (color >> 24) & 0xFF;
	g = (color >> 16) & 0xFF;
	b = (color >> 8) & 0xFF;
	a = color & 0xFF;
}

// Get current time in microseconds (monotonic)
inline uint64_t get_time_us() {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

// Get current real-time in microseconds (for synchronization)
inline uint64_t get_realtime_us() {
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

// Sleep until a specific real-time timestamp
inline void sleep_until_realtime_us(uint64_t target_us) {
	struct timespec target_ts;
	target_ts.tv_sec = target_us / 1000000;
	target_ts.tv_nsec = (target_us % 1000000) * 1000;
	clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &target_ts, nullptr);
}

#endif // _DEMO_COMMON_H
