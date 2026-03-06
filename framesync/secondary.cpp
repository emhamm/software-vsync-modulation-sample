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
#include <queue>
#include <mutex>
#include <condition_variable>
#include <cerrno>
#include <cstring>

struct PendingUpdate {
	DemoColorUpdateMsg msg;
	uint64_t received_at_vblank;
};

static std::queue<PendingUpdate> g_pending_updates;
static std::mutex g_update_mutex;
static std::condition_variable g_update_cv;

/**
 * Thread to receive color update messages from primary via UDP multicast
 */
static void receive_updates_thread(int udp_sockfd, Renderer* renderer) {
	(void)renderer; // Reserved for future use
	while (!g_shutdown) {
		DemoMessage msg;

		// Try to receive message with timeout
		fd_set read_fds;
		FD_ZERO(&read_fds);
		FD_SET(udp_sockfd, &read_fds);

		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 100000;  // 100ms timeout

		int ret = select(udp_sockfd + 1, &read_fds, nullptr, nullptr, &tv);
		if (ret < 0) {
			if (!g_shutdown) {
				ERR("select() failed in receive thread\n");
			}
			break;
		}

		if (ret == 0) {
			// Timeout - continue waiting
			continue;
		}

		// Receive multicast message
		struct sockaddr_in src_addr;
		socklen_t addr_len = sizeof(src_addr);
		ssize_t received = recvfrom(udp_sockfd, &msg, sizeof(msg), 0,
		                            (struct sockaddr*)&src_addr, &addr_len);
		if (received <= 0) {
			if (!g_shutdown) {
				ERR("Failed to receive multicast message\n");
			}
			continue;
		}

		if (msg.type == MSG_COLOR_UPDATE) {
			// Get current realtime when message received
			uint64_t receive_time = get_realtime_us();

			PendingUpdate update;
			update.msg = msg.payload.color_update;
			update.received_at_vblank = receive_time;  // Repurpose for timestamp

			{
				std::lock_guard<std::mutex> lock(g_update_mutex);
				g_pending_updates.push(update);
			}
			g_update_cv.notify_one();

			int64_t time_until_target = (int64_t)update.msg.target_timestamp_us - (int64_t)receive_time;
			DBG("Received multicast update #%lu from %s: target_time=%lu us, color=0x%08X (%+ld ms until target)\n",
				update.msg.sequence_num, inet_ntoa(src_addr.sin_addr),
				update.msg.target_timestamp_us, update.msg.color, time_until_target / 1000);
		}
	}
}

/**
 * Secondary client main function
 */
int run_secondary(const DemoConfig& config) {
	INFO("Starting secondary mode (client)\n");
	INFO("  Multicast: %s:%d\n", config.multicast_group.c_str(), config.multicast_port);
	INFO("  Pipe: %d\n", config.pipe);

	// Create UDP socket for receiving multicast color updates
	int udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (udp_sockfd < 0) {
		ERR("Failed to create UDP socket\n");
		return -1;
	}

	// Allow multiple bindings to the same port (for multiple receivers on same host)
	int reuse = 1;
	if (setsockopt(udp_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		WARNING("Failed to set SO_REUSEADDR on UDP socket\n");
	}

	// Bind to multicast port
	struct sockaddr_in local_addr;
	memset(&local_addr, 0, sizeof(local_addr));
	local_addr.sin_family = AF_INET;
	local_addr.sin_addr.s_addr = INADDR_ANY;
	local_addr.sin_port = htons(config.multicast_port);

	if (bind(udp_sockfd, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
		ERR("Failed to bind UDP socket to port %d\n", config.multicast_port);
		close(udp_sockfd);
		return -1;
	}

	// Join multicast group
	struct ip_mreq mreq;
	if (inet_pton(AF_INET, config.multicast_group.c_str(), &mreq.imr_multiaddr) <= 0) {
		ERR("Invalid multicast address: %s\n", config.multicast_group.c_str());
		close(udp_sockfd);
		return -1;
	}

	// Set receiving interface
	if (!config.multicast_if.empty()) {
		if (inet_pton(AF_INET, config.multicast_if.c_str(), &mreq.imr_interface) <= 0) {
			ERR("Invalid multicast interface address: %s\n", config.multicast_if.c_str());
			close(udp_sockfd);
			return -1;
		}
		INFO("Using multicast interface: %s\n", config.multicast_if.c_str());
	} else {
		mreq.imr_interface.s_addr = INADDR_ANY;
		INFO("Using multicast interface: INADDR_ANY (auto-select)\n");
	}

	if (setsockopt(udp_sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
		ERR("Failed to join multicast group %s: %s\n",
		    config.multicast_group.c_str(), strerror(errno));
		close(udp_sockfd);
		return -1;
	}

	INFO("Joined multicast group %s on port %d\n",
	     config.multicast_group.c_str(), config.multicast_port);

	// Initialize renderer
	Renderer renderer(config);
	if (renderer.init() != 0) {
		ERR("Failed to initialize renderer\n");
		close(udp_sockfd);
		return -1;
	}

	// Start receive thread for multicast updates
	std::thread recv_thread(receive_updates_thread, udp_sockfd, &renderer);

	// Main loop: apply color updates at target timestamps
	INFO("Starting synchronization\n");

	// Render initial black screen
	renderer.render_solid_color(0x000000FF);
	renderer.present();

	while (!g_shutdown) {
		// Check for window close
		if (renderer.should_close()) {
			INFO("Window close requested\n");
			break;
		}

		// Check for pending updates
		PendingUpdate update;
		bool have_update = false;

		{
			std::unique_lock<std::mutex> lock(g_update_mutex);
			if (!g_pending_updates.empty()) {
				update = g_pending_updates.front();
				g_pending_updates.pop();
				have_update = true;
			}
		}

		if (have_update) {
			uint64_t now = get_realtime_us();
			int64_t time_until_target = (int64_t)update.msg.target_timestamp_us - (int64_t)now;

			if (time_until_target < -50000) {  // More than 50ms late
				WARNING("Missed target timestamp by %ld ms for sequence #%lu\n",
					-time_until_target / 1000, update.msg.sequence_num);
			} else if (time_until_target > 0) {
				// Wait until target timestamp
				sleep_until_realtime_us(update.msg.target_timestamp_us);
			}
			// If we're slightly late (< 50ms), continue immediately

			// Wait for next vblank after target time for tear-free presentation
			uint64_t present_vblank;
			if (renderer.wait_for_vblank(present_vblank) == 0) {
				// Render new color at the vblank
				renderer.render_solid_color(update.msg.color);
				renderer.present();

				uint64_t actual_time = get_realtime_us();
				int64_t drift_us = (int64_t)actual_time - (int64_t)update.msg.target_timestamp_us;

				INFO("Color changed to 0x%08X (sequence: #%lu, drift: %+ld us, vblank: %lu)\n",
					update.msg.color, update.msg.sequence_num, drift_us, present_vblank);
			} else {
				ERR("Failed to wait for vblank for color update\n");
			}
		} else {
			// No pending updates, just wait for next vblank to reduce CPU usage
			uint64_t vblank_timestamp;
			if (renderer.wait_for_vblank(vblank_timestamp) != 0) {
				ERR("Failed to wait for vblank\n");
				break;
			}
		}
	}

	INFO("Shutting down secondary\n");

	// Cleanup
	g_shutdown = true;
	recv_thread.join();

	// Leave multicast group before closing
	struct ip_mreq mreq_leave;
	if (inet_pton(AF_INET, config.multicast_group.c_str(), &mreq_leave.imr_multiaddr) <= 0) {
		WARNING("Failed to convert multicast address for leaving group\n");
	} else {
		mreq_leave.imr_interface.s_addr = INADDR_ANY;
		if (setsockopt(udp_sockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq_leave, sizeof(mreq_leave)) < 0) {
			WARNING("Failed to leave multicast group: %s\n", strerror(errno));
		}
	}

	close(udp_sockfd);
	renderer.cleanup();

	return 0;
}
