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

#include "demo_common.h"
#include "renderer.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <vector>
#include <mutex>
#include <fcntl.h>
#include <cerrno>
#include <cstring>

static int g_multicast_fd = -1;
static struct sockaddr_in g_multicast_addr;

/**
 * Send color update message to all connected clients via UDP multicast
 */
static void broadcast_color_update(const DemoColorUpdateMsg& msg) {
	if (g_multicast_fd < 0) {
		ERR("Multicast socket not initialized\n");
		return;
	}

	DemoMessage demo_msg = {};
	demo_msg.type = MSG_COLOR_UPDATE;
	demo_msg.payload_size = sizeof(DemoColorUpdateMsg);
	demo_msg.payload.color_update = msg;

	// Send via UDP multicast - single packet reaches all subscribers
	ssize_t sent = sendto(g_multicast_fd, &demo_msg, sizeof(demo_msg), 0,
						  (struct sockaddr*)&g_multicast_addr, sizeof(g_multicast_addr));
	if (sent < 0) {
		ERR("Failed to send multicast packet: %s\n", strerror(errno));
	} else {
		DBG("Multicast sent: %zd bytes to %s:%d\n", sent,
			inet_ntoa(g_multicast_addr.sin_addr), ntohs(g_multicast_addr.sin_port));
	}
}

/**
 * Primary coordinator main function
 */
int run_primary(const DemoConfig& config) {
	INFO("Starting primary mode (coordinator)\n");
	INFO("  Multicast: %s:%d (TTL=%d)\n", config.multicast_group.c_str(),
		 config.multicast_port, config.multicast_ttl);
	INFO("  Pipe: %d\n", config.pipe);
	INFO("  Interval: %d ms\n", config.interval_ms);
	INFO("  Schedule ahead: %d ms\n", config.schedule_ahead_ms);

	// Create UDP multicast socket for color updates
	g_multicast_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (g_multicast_fd < 0) {
		ERR("Failed to create multicast socket\n");
		return -1;
	}

	// Configure multicast address
	memset(&g_multicast_addr, 0, sizeof(g_multicast_addr));
	g_multicast_addr.sin_family = AF_INET;
	g_multicast_addr.sin_port = htons(config.multicast_port);
	if (inet_pton(AF_INET, config.multicast_group.c_str(), &g_multicast_addr.sin_addr) <= 0) {
		ERR("Invalid multicast address: %s\n", config.multicast_group.c_str());
		close(g_multicast_fd);
		return -1;
	}

	// Set multicast TTL
	unsigned char ttl = config.multicast_ttl;
	if (setsockopt(g_multicast_fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
		ERR("Failed to set multicast TTL\n");
		close(g_multicast_fd);
		return -1;
	}

	// Enable multicast loopback (always enabled to support mixed deployments)
	// This allows processes on the same machine to receive multicasts
	// Overhead is negligible and ensures compatibility with local + network clients
	unsigned char loop = 1;
	if (setsockopt(g_multicast_fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop)) < 0) {
		WARNING("Failed to set multicast loopback\n");
	}
	INFO("Multicast loopback: enabled (supports mixed local+network deployment)\n");

	// Set outgoing multicast interface if specified
	if (!config.multicast_if.empty()) {
		struct in_addr local_interface;
		if (inet_pton(AF_INET, config.multicast_if.c_str(), &local_interface) <= 0) {
			ERR("Invalid multicast interface address: %s\n", config.multicast_if.c_str());
			close(g_multicast_fd);
			return -1;
		}
		if (setsockopt(g_multicast_fd, IPPROTO_IP, IP_MULTICAST_IF, &local_interface, sizeof(local_interface)) < 0) {
			ERR("Failed to set multicast interface to %s\n", config.multicast_if.c_str());
			close(g_multicast_fd);
			return -1;
		}
		INFO("Multicast interface: %s\n", config.multicast_if.c_str());
	} else {
		INFO("Multicast interface: INADDR_ANY (auto-select)\n");
	}

	INFO("Multicast socket ready: %s:%d (TTL=%d)\n",
		 config.multicast_group.c_str(), config.multicast_port, config.multicast_ttl);

	// Initialize renderer
	Renderer renderer(config);
	if (renderer.init() != 0) {
		ERR("Failed to initialize renderer\n");
		close(g_multicast_fd);
		return -1;
	}

	// Main loop: coordinate color changes
	uint64_t sequence_num = 0;
	uint32_t color_index = 0;
	uint64_t last_change_time = get_time_us();

	// Render initial color
	renderer.render_solid_color(DEMO_COLORS[color_index]);
	renderer.present();

	INFO("Starting color coordination (press ESC or Ctrl+C to quit)\n");

	while (!g_shutdown) {
		// Check for window close
		if (renderer.should_close()) {
			INFO("Window close requested\n");
			break;
		}

		// Wait for vblank
		uint64_t current_vblank;
		if (renderer.wait_for_vblank(current_vblank) != 0) {
			ERR("Failed to wait for vblank\n");
			break;
		}

		// Add 500us buffer after vblank
		usleep(500);

		// Check if it's time to schedule next color change
		uint64_t now = get_time_us();
		uint64_t elapsed = now - last_change_time;

		if (elapsed >= static_cast<uint64_t>(config.interval_ms) * 1000) {
			// Time to schedule next color change
			if (config.cycle_colors) {
				color_index = (color_index + 1) % DEMO_NUM_COLORS;
			}

			uint32_t next_color = DEMO_COLORS[color_index];

			// Calculate target timestamp: now + schedule_ahead_ms
			// We're right after a vblank, so this gives maximum time for message delivery
			uint64_t target_timestamp = get_realtime_us() + (config.schedule_ahead_ms * 1000ULL);

			// Create color update message
			DemoColorUpdateMsg update_msg;
			update_msg.target_timestamp_us = target_timestamp;
			update_msg.color = next_color;
			update_msg.pattern_type = 0;  // Solid color
			update_msg.sequence_num = ++sequence_num;

			// Broadcast to all clients (sent right after vblank + 500us)
			broadcast_color_update(update_msg);

			struct timespec ts;
			ts.tv_sec = target_timestamp / 1000000;
			ts.tv_nsec = (target_timestamp % 1000000) * 1000;
			char time_str[64];
			strftime(time_str, sizeof(time_str), "%H:%M:%S", localtime(&ts.tv_sec));

			DBG("Sent update #%lu: color=0x%08X, target=%s.%06lu (+%dms), vblank=%lu\n",
				sequence_num, next_color, time_str, target_timestamp % 1000000,
				config.schedule_ahead_ms, current_vblank);

			// Wait until target timestamp
			sleep_until_realtime_us(target_timestamp);

			// Wait for the next vblank to ensure frame-accurate, tear-free presentation
			uint64_t present_vblank;
			if (renderer.wait_for_vblank(present_vblank) == 0) {
				// Render new color at the vblank
				renderer.render_solid_color(next_color);
				renderer.present();

				uint64_t actual_time = get_realtime_us();
				int64_t drift_us = (int64_t)actual_time - (int64_t)target_timestamp;
				DBG("Color changed to 0x%08X (drift: %+ld us, vblank: %lu)\n",
					next_color, drift_us, present_vblank);
			}

			last_change_time = get_time_us();
		}
	}

	INFO("Shutting down primary\n");

	// Cleanup
	if (g_multicast_fd >= 0) {
		close(g_multicast_fd);
		g_multicast_fd = -1;
	}

	renderer.cleanup();

	return 0;
}
