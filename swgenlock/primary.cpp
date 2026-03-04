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

#include "sync_common.h"
#include "network_platform.h"
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

std::atomic<bool> g_collecting{false};
std::deque<uint64_t> g_vsync_buffer;
std::mutex g_vsync_mutex;

// Monitoring thread globals
std::atomic<bool> g_monitoring{false};
std::thread g_monitor_thread;
int g_monitor_sockfd = -1;
std::ofstream g_monitor_csv;
std::mutex g_csv_mutex;

struct client_thread_args {
	int sockfd;
	int pipe;
};

/**
 * @brief
 * Background thread that continuously collects vblank timestamps into a buffer.
 * This function runs in a separate thread and maintains a rolling buffer of
 * vsync timestamps by continuously calling get_vsync(). When the buffer reaches
 * max_count, the oldest timestamp is removed to make room for the newest one.
 * @param device_str - The device string identifier (e.g., "/dev/dri/card0")
 * @param pipe - The pipe number to collect vblank timestamps from
 * @param max_count - Maximum number of timestamps to store in the buffer
 * @return void
 */
void vsync_collector_thread(const char* device_str, int pipe, size_t max_count)
{
	uint64_t ts;
	g_collecting = true;
	while (g_collecting) {
		if (get_vsync(device_str, &ts, 1, pipe) == 0) {
			std::lock_guard<std::mutex> lock(g_vsync_mutex);
			if (g_vsync_buffer.size() >= max_count) {
				g_vsync_buffer.pop_front();
			}
			g_vsync_buffer.push_back(ts);
		}
	}
}

/**
 * @brief
 * Starts the vsync collector thread in the background.
 * Creates and detaches a thread that continuously collects vblank timestamps.
 * The thread will run until stop_vsync_collector() is called.
 * @param device_str - The device string identifier (e.g., "/dev/dri/card0")
 * @param pipe - The pipe number to collect vblank timestamps from
 * @param max_count - Maximum number of timestamps to store in the buffer
 * @return void
 */
void start_vsync_collector(const char* device_str, int pipe, size_t max_count)
{
	std::thread collector(vsync_collector_thread, device_str, pipe, max_count);
	collector.detach();
}

/**
 * @brief
 * Stops the vsync collector thread.
 * Sets the global flag to stop the background collection thread.
 * The thread will exit after completing its current collection cycle.
 * @param None
 * @return void
 */
void stop_vsync_collector()
{
	g_collecting = false;
}

/**
 * @brief
 * Fetches the last 'count' vsync timestamps from the buffer.
 * Retrieves the most recent timestamps from the global vsync buffer in
 * chronological order. [0] -> oldest and [count - 1] -> latest timestamp.
 * Thread-safe access is ensured via mutex lock.
 * @param dest - Destination array to copy timestamps into
 * @param count - Number of timestamps to retrieve
 * @return
 * - true if successfully retrieved 'count' timestamps
 * - false if buffer doesn't have enough timestamps yet
 */
bool get_last_vsyncs(uint64_t* dest, size_t count)
{
	std::lock_guard<std::mutex> lock(g_vsync_mutex);
	if (g_vsync_buffer.size() < count) {
		return false;
	}
	auto it = g_vsync_buffer.end() - count;
	std::copy(it, g_vsync_buffer.end(), dest);
	return true;
}

/**
 * @brief
 * Generate CSV filename with timestamp for sync monitoring
 * @return Filename string in format: swgenlock_monitor_<timestamp>.csv
 */
static std::string generate_monitor_csv_filename()
{
	time_t now_time = time(nullptr);
	struct tm *tm_info = localtime(&now_time);
	char timestamp[32];

	if (tm_info) {
		strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_info);
	} else {
		// Fallback if localtime fails
		snprintf(timestamp, sizeof(timestamp), "%ld", (long)now_time);
	}

	std::ostringstream oss;
	oss << "swgenlock_monitor_" << timestamp << ".csv";
	return oss.str();
}

/**
 * @brief
 * Monitoring thread that listens for sync status updates from secondaries
 * Logs received status to CSV file with timestamp, node name, pipe, and delta
 * @param monitor_ip IP address to bind the monitoring server
 * @param monitor_port Port to listen on for sync status updates
 */
static void sync_monitor_thread_func(std::string monitor_ip, int monitor_port)
{
	INFO("Starting sync monitoring thread on %s:%d\n", monitor_ip.c_str(), monitor_port);

	// Open CSV file for writing with timestamp
	std::string csv_filename = generate_monitor_csv_filename();

	{
		std::lock_guard<std::mutex> lock(g_csv_mutex);
		g_monitor_csv.open(csv_filename, std::ios::out | std::ios::trunc);
		if (!g_monitor_csv.is_open()) {
			ERR("Failed to open monitor CSV file: %s\n", csv_filename.c_str());
			return;
		}
		// Write CSV header
		g_monitor_csv << "timestamp_us,node_name,pipe,delta_us" << std::endl;
		INFO("Monitor CSV file created: %s\n", csv_filename.c_str());
	}

	// Create UDP socket for monitoring
	g_monitor_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (g_monitor_sockfd < 0) {
		ERR("Failed to create monitor socket\n");
		{
			std::lock_guard<std::mutex> lock(g_csv_mutex);
			g_monitor_csv.close();
		}
		return;
	}

	// Set socket to non-blocking
	int flags = fcntl(g_monitor_sockfd, F_GETFL, 0);
	if (flags >= 0) {
		if (fcntl(g_monitor_sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
			WARNING("Failed to set monitor socket to non-blocking mode\n");
		}
	} else {
		WARNING("Failed to get monitor socket flags\n");
	}

	struct sockaddr_in monitor_addr;
	memset(&monitor_addr, 0, sizeof(monitor_addr));
	monitor_addr.sin_family = AF_INET;
	monitor_addr.sin_port = htons(monitor_port);
	monitor_addr.sin_addr.s_addr = inet_addr(monitor_ip.c_str());

	if (bind(g_monitor_sockfd, (struct sockaddr*)&monitor_addr, sizeof(monitor_addr)) < 0) {
		ERR("Failed to bind monitor socket to port %d\n", monitor_port);
		close(g_monitor_sockfd);
		g_monitor_sockfd = -1;
		{
			std::lock_guard<std::mutex> lock(g_csv_mutex);
			g_monitor_csv.close();
		}
		return;
	}

	INFO("Sync monitor listening on port %d, logging to %s\n", monitor_port, csv_filename.c_str());

	// Listen for sync status messages
	while (g_monitoring) {
		sync_status_msg status_msg;
		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);

		ssize_t recv_len = recvfrom(g_monitor_sockfd, &status_msg, sizeof(status_msg), 0,
									  (struct sockaddr*)&client_addr, &client_len);

		if (recv_len == sizeof(sync_status_msg) && status_msg.header == SYNC_STATUS_MSG) {
			// Ensure hostname is null-terminated
			status_msg.hostname[MAX_HOSTNAME_LEN - 1] = '\0';

			// Write to CSV file
			std::lock_guard<std::mutex> lock(g_csv_mutex);
			g_monitor_csv << status_msg.timestamp_us << ","
						  << status_msg.hostname << ","
						  << status_msg.pipe << ","
						  << status_msg.delta_us << std::endl;
			g_monitor_csv.flush();  // Ensure data is written immediately

			DBG("Monitor: %s pipe %d delta %ld us at %lu\n",
				status_msg.hostname, status_msg.pipe, status_msg.delta_us, status_msg.timestamp_us);
		} else if (recv_len < 0 && errno != EWOULDBLOCK && errno != EAGAIN) {
			// Real error (not just no data available)
			ERR("Monitor recvfrom error: %s\n", strerror(errno));
		}

		// Sleep a bit to avoid busy waiting
		os_usleep(10000);  // 10ms
	}

	// Cleanup
	if (g_monitor_sockfd >= 0) {
		close(g_monitor_sockfd);
		g_monitor_sockfd = -1;
	}

	{
		std::lock_guard<std::mutex> lock(g_csv_mutex);
		g_monitor_csv.close();
		INFO("Monitor CSV file closed: %s\n", csv_filename.c_str());
	}

	INFO("Sync monitoring thread stopped\n");
}

/**
 * @brief
 * Start the sync status monitoring thread
 * @param primary_ip IP address of primary to bind monitoring server
 * @param primary_port Primary sync port (monitoring will use primary_port + MONITOR_PORT_OFFSET)
 */
void start_sync_monitor_thread(const char* primary_ip, int primary_port)
{
	if (g_monitoring) {
		WARNING("Monitoring thread already running\n");
		return;
	}

	int monitor_port = primary_port + MONITOR_PORT_OFFSET;
	g_monitoring = true;
	g_monitor_thread = std::thread(sync_monitor_thread_func, std::string(primary_ip), monitor_port);
}

/**
 * @brief
 * Stop the sync status monitoring thread
 */
void stop_sync_monitor_thread()
{
	if (!g_monitoring) {
		return;
	}

	g_monitoring = false;

	if (g_monitor_thread.joinable()) {
		g_monitor_thread.join();
	}
}

/**
* @brief
* This function is used by the server side to get last 10 vsyncs, send
* them to the client and receive an ACK from it. Once reveived, it terminates
* connection with the client.
* @param new_sockfd - The socket on which we need to communicate with the client
* @param pipe - The pipe used for communication
* @return
* - 0 = SUCCESS
* - 1 = FAILURE
*/
int do_msg(int new_sockfd, int pipe)
{
	int ret = 0;
	msg m, r;
	uint64_t* va = m.get_va();
	INFO("Handling request for pipe %d\n", pipe);
	do {
		memset(&m, 0, sizeof(m));
		if (server->recv_msg(&r, sizeof(r), new_sockfd)) {
			ret = 1;
			break;
		}

		// Validate vblank count from network data to prevent buffer overflow
		int vblank_count = r.get_vblank_count();
		if (vblank_count <= 0 || vblank_count > VSYNC_MAX_TIMESTAMPS) {
			ERR("Invalid vblank count received: %d (max: %d)\n", vblank_count, VSYNC_MAX_TIMESTAMPS);
			ret = 1;
			break;
		}

		if (!get_last_vsyncs(va, vblank_count)) {
			ERR("Not enough vsync data available yet");
			ret = 1;
			break;
		}

		print_vsyncs((char *)"", va, vblank_count);
		m.add_vsync();
		m.add_time();

		if (server->send_msg(&m, sizeof(m), new_sockfd)) {
			ret = 1;
			break;
		}
	} while (r.get_type() != ACK);

	if (new_sockfd) {
		os_close_socket(new_sockfd);
	}

	return ret;
}

/**
 * @brief
 * Thread entry point for handling a single client connection.
 * Extracts client information from the thread arguments, processes the
 * client's request via do_msg(), and cleans up allocated resources.
 * @param arg - Pointer to client_thread_args structure containing socket and pipe info
 * @return nullptr
 */
void* client_handler(void* arg)
{
	client_thread_args* args = (client_thread_args*)arg;

	int sockfd = args->sockfd;
	int pipe = args->pipe;

	delete args;  // Clean up heap allocation

	// Set thread-local pipe context for automatic logging identification
	set_thread_pipe_id(pipe);

	do_msg(sockfd, pipe);  // Now pass the pipe to do_msg
	return nullptr;
}

/**
* @brief
* This function takes all the actions of the primary system which
* are to initialize the server, wait for any clients, dispatch a function to
* handle each client and then wait for another client to join until the user
* Ctrl+C's out.
* @param *ptp_if - This is the PTP interface provided by the user on command
* line. It can also be NULL, in which case they would rather have us
* communicate via TCP.
* @return
* - 0 = SUCCESS
* - 1 = FAILURE
*/
int do_primary(const char* ptp_if, int pipe)
{
	const int N_EXTRA = 10;
	bool is_ptp = !is_valid_ip4(std::string(ptp_if));

	if(is_ptp) {
		server = new ptp_connection(ptp_if);
	} else {
		server = new connection(ptp_if);
	}

	if (server->init_server()) {
		ERR("Failed to init socket connection\n");
		return 1;
	}

	signal(SIGINT, server_close_signal);
	signal(SIGTERM, server_close_signal);

	// Start continuous vsync collection
	start_vsync_collector(g_devicestr, pipe, VSYNC_MAX_TIMESTAMPS + N_EXTRA);

	// Start sync monitoring if enabled
	if (g_enable_monitoring) {
		// Extract IP and port from server connection
		const char* server_ip = ptp_if;
		int server_port = 5000;  // Default swgenlock port
		start_sync_monitor_thread(server_ip, server_port);
	}

	// Give some time to get queue filled up
	os_sleep_ms(1000);

	int ret = 0;
	while (1) {
		int new_socket;
		if (server->accept_client(&new_socket)) {
			ret = 1;
			break;
		}

		// Allocate args on the heap to pass into the thread
		client_thread_args* args = new client_thread_args();
		args->sockfd = new_socket;
		args->pipe = pipe;

		pthread_t thread_id;
		if (pthread_create(&thread_id, nullptr, client_handler, args)) {
			ERR("Failed to create thread for new client");
			os_close_socket(new_socket);
			delete args;
			continue;
		}

		if (is_ptp) {
			// In PTP mode, wait for thread to complete before accepting next packet
			pthread_join(thread_id, nullptr);
		} else {
			// TCP mode: allow parallel clients
			pthread_detach(thread_id);
		}
	}

	// Cleanup
	if (g_enable_monitoring) {
		stop_sync_monitor_thread(); // Stop last started thread first
	}

	stop_vsync_collector(); // Stop first started thread last
	return ret;
}
