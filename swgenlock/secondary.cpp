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
#include <unistd.h>

const double US_IN_MS = 1000.0;

/**
 * @brief
 * Find the minimum delta between primary and secondary timestamps
 *
 * @param primary_vsync Primary timestamps array
 * @param secondary_vsync Secondary timestamps array
 * @param timestamps Number of secondary timestamps
 * @param timestamps_from_primary Number of primary timestamps
 * @param matching_timestamp Output parameter for matching primary timestamp
 * @return Minimum delta in microseconds
 */
static long find_minimum_delta(
	const uint64_t *primary_vsync,
	const uint64_t *secondary_vsync,
	int timestamps,
	int timestamps_from_primary,
	uint64_t *matching_timestamp)
{
	long delta = LONG_MAX;

	/*
	 * Find the nearest timestamps between primary and secondary vblank timestamps
	 * to calculate the delta (time offset) between the two systems.
	 *
	 * Network Delay Compensation:
	 * Earlier in the code the secondary system first captures its local vblank timestamps, then sends
	 * a request to the primary system. By the time the primary receives the request
	 * and sends back its timestamps, some time has elapsed (e.g., ~100ms network delay).
	 * During this delay, several vblanks have already occurred on both systems.
	 *
	 * To compensate for this network delay, we request N_EXTRA (10) additional timestamps
	 * from the primary. On a 60Hz refresh rate, these extra 10 timestamps cover ~160ms
	 * (10 * 16.66ms), ensuring we have primary timestamps that overlap with the secondary
	 * timestamps despite the network delay.
	 *
	 * The nested loop searches through all timestamp pairs to find the minimal delta,
	 * which represents the closest synchronization point between the two systems' vblanks.
	 *
	 * TODO: The search goes through all timestamps even if delta was found at the start.
	 * 		 This can be improved in future.
	 */
	for (int i = timestamps - 1; i >= 0; --i) {
		for (int j = timestamps_from_primary - 1; j >= 0; --j) {
			int64_t diff = (uint64_t)secondary_vsync[i] - (uint64_t)primary_vsync[j];
			if (labs(diff) < labs(delta)) {
				delta = diff;
				*matching_timestamp = primary_vsync[j];
			}
		}
	}

	return delta;
}

/**
 * @brief
 * Normalize delta to align with nearest vsync interval
 *
 * @param delta Current delta value
 * @param avg_secondary Average secondary vsync interval
 * @return Normalized delta
 */
static long normalize_delta(long delta, long avg_secondary)
{
	/*
	 * If the primary is ahead or behind the secondary by more than a vsync,
	 * we can just adjust the secondary's vsync to what we think the primary's
	 * next vsync would be happening at. We do this by calculating the average
	 * of its last N vsyncs that it has provided to us and assuming that its
	 * next vsync would be happening at this cadence. Then we modulo the delta
	 * with this average to give us a time difference between it's nearest
	 * vsync to the secondary's. As long as we adjust the secondary's vsync
	 * to this value, it would basically mean that the primary and secondary
	 * system's vsyncs are firing at the same time.
	 */
	if(delta > avg_secondary || delta < avg_secondary) {
		delta %= avg_secondary;
	}

	/*
	 * If the time difference between primary and secondary is larger than the
	 * mid point of secondary's vsync time period, then it makes sense to sync
	 * with the next vsync of the primary. For example, say primary's vsyncs
	 * are happening at a regular cadence of 0, 16.66, 33.33 ms while
	 * secondary's vsyncs are happening at a regular cadence of 10, 26.66,
	 * 43.33 ms, then the time difference between them is exactly 10 ms. If
	 * we were to make the secondary faster for each iteration so that it
	 * walks back those 10 ms and gets in sync with the primary, it would have
	 * taken us about 600 iterations of vsyncs = 10 seconds. However, if were
	 * to make the secondary slower for each iteration so that it walks forward
	 * then since it is only 16.66 - 10 = 6.66 ms away from the next vsync of
	 * the primary, it would take only 400 iterations of vsyncs = 6.66 seconds
	 * to get in sync. Therefore, the general rule is that we should make
	 * secondary's vsyncs walk back only if it the delta is less than half of
	 * secondary's vsync time period, otherwise, we should walk forward.
	 */
	if(delta > avg_secondary/2) {
		delta -= avg_secondary;
	}

	return delta;
}

/**
 * @brief
 * This function is the background thread task to call print_vblank_interval
 *
 * @param arg Pointer to pipe number (int)
 * @return void*
 */
void* background_task(void* arg)
{
	const int WAIT_TIME_IN_MICRO = 1000 * 1000;  // 1 sec
	double avg_interval;
	int pipe = *(int*)arg;

	// Set thread-local pipe context for automatic logging identification
	set_thread_pipe_id(pipe);

	INFO("VBlank interval during synchronization ->\n");
	os_timespec start_time, current_time;
	os_clock_gettime(OS_CLOCK_MONOTONIC, &start_time);

	// Thread runs this loop until it is canceled (check per-pipe thread_continue)
	PipeStats stats;
	get_pipe_stats(pipe, &stats);
	while (stats.thread_continue) {
		os_usleep(WAIT_TIME_IN_MICRO);
		// Calculate elapsed time
		os_clock_gettime(OS_CLOCK_MONOTONIC, &current_time);
		double elapsed_time = (current_time.tv_sec - start_time.tv_sec) +
			(current_time.tv_nsec - start_time.tv_nsec) / 1e9;

		avg_interval = get_vblank_interval(g_devicestr, pipe, 30);
		INFO("\t[Elapsed Time: %.2lf sec] VBlank interval on pipe %d is %.4lf ms\n", elapsed_time, pipe, avg_interval);

		// Re-check the per-pipe thread_continue flag
		get_pipe_stats(pipe, &stats);
	}

	avg_interval = get_vblank_interval(g_devicestr, pipe, 30);
	INFO("VBlank interval after synchronization ends: %.4lf ms\n", avg_interval);
	return nullptr;
}

/**
 * @brief
 * Perform the actual synchronization with optional adaptive learning
 *
 * @param delta_ms Delta in milliseconds
 * @param pipe Pipe ID
 * @param shift PLL shift value
 * @param shift2 PLL shift2 value
 * @param step_threshold Step threshold
 * @param wait_between_steps Wait time between steps
 * @param learning_rate Learning rate for adaptive learning
 * @param time_period Time period for learning
 * @param overshoot_ratio Overshoot ratio
 * @param duration Duration since last sync
 * @param sync_count Number of successful syncs
 * @param success_iter Number of successful iterations
 * @return 0 on success, 1 on failure
 */
static int perform_synchronization(
	double delta_ms,
	int pipe,
	double shift,
	double shift2,
	int step_threshold,
	int wait_between_steps,
	double learning_rate,
	int time_period,
	double overshoot_ratio,
	int64_t duration,
	u_int32_t sync_count,
	u_int32_t success_iter)
{
	pthread_t tid;
	int status;

	// Set per-pipe thread_continue flag to 1
	PipeStats stats;
	get_pipe_stats(pipe, &stats);
	stats.thread_continue = 1;
	set_pipe_stats(pipe, &stats);

	// synchronize_vsync function is synchronous call and does not
	// output any information. To enhance visibility, a thread is
	// created for logging vblank intervals while synchronization is
	// in progress.
	status = pthread_create(&tid, nullptr, background_task, &pipe);
	if (status != 0) {
		ERR("Thread creation failed");
		return 1;
	}

	// Apply overshoot ratio if learning is disabled or beyond time period
	double adjusted_delta_ms = delta_ms;
	if(!is_not_zero(learning_rate) || duration > ((int64_t)time_period * (int64_t)US_IN_MS)) {
		adjusted_delta_ms *= (1.0 + overshoot_ratio); // Apply overshoot ratio
	}

	synchronize_vsync(adjusted_delta_ms, pipe, shift, shift2, step_threshold, wait_between_steps, true, true);

	// Set per-pipe thread_continue flag to 0 to signal the thread to terminate
	get_pipe_stats(pipe, &stats);
	stats.thread_continue = 0;
	set_pipe_stats(pipe, &stats);

	pthread_join(tid, nullptr); // Wait for the thread to terminate

	// Adaptive Learning:
	// If learning rate is provided and atleast two syncs were triggered before (allow for atleast one sync)
	// and had atleast one successful check and the duration since the last synchronization is less
	// than the specified time period, adjust the clocks using the learning rate.
	// The check for sync_count > 1 ensures that we only start apply learning after the second synchronization,
	// allowing the system to stabilize.


	if (is_not_zero(learning_rate) && sync_count > 1
			&& duration < ((int64_t)(time_period * US_IN_MS))) {

		os_usleep(100*1000); // Give some time for the clocks to adjust from earlier sync
		INFO("Adaptive Learning after %.3f secs (%d iterations)\n", duration / US_IN_MS, success_iter);

		// delta_ms is used to determine the adjustment direction (increase or decrease).
		// The learning_rate is used as the shift value.  The 'false' parameter indicates
		// not to reset values after writing and to return immediately.
		synchronize_vsync((double) delta_ms, pipe, learning_rate, 0.0, step_threshold, wait_between_steps, false, true);
	}

	return 0;
}

/**
 * @brief
 * Capture vblank timestamps from primary via network (TCP/PTP)
 *
 * @param timestamps_from_primary Number of timestamps to request from primary
 * @param primary_vsync Array to store primary timestamps
 * @param roundtrip_us Output parameter for round-trip time in microseconds
 * @param server_ip Server IP address
 * @param eth_addr Ethernet address for PTP (NULL for TCP)
 * @return 0 on success, 1 on failure
 */
static int capture_primary_vsync_network(
	int timestamps_from_primary,
	uint64_t *primary_vsync,
	uint64_t *roundtrip_us,
	const char *server_ip,
	const char *eth_addr)
{
	// Create client connection (per-call, destroyed on exit)
	connection *client_conn = eth_addr ? new ptp_connection(server_ip, eth_addr)
										: new connection(server_ip);

	if (client_conn->init_client(server_ip)) {
		delete client_conn;
		return 1;
	}

	msg m, r;
	int ret;

	uint64_t start_time = get_realtime_timestamp_us();

	do {
		r.ack();
		r.set_vblank_count(timestamps_from_primary);
		ret = client_conn->send_msg(&r, sizeof(r)) || client_conn->recv_msg(&m, sizeof(m));
	} while (ret);

	uint64_t end_time = get_realtime_timestamp_us();
	*roundtrip_us = end_time - start_time;

	memcpy(primary_vsync, m.get_va(), sizeof(uint64_t) * timestamps_from_primary);
	print_vsyncs((char*)"PRIMARY", primary_vsync, timestamps_from_primary);

	// Cleanup: close and delete client connection
	client_conn->close_client();
	delete client_conn;

	return 0;
}

/**
 * @brief
 * Capture vblank timestamps from local primary collector (pipelock mode)
 *
 * @param timestamps_from_primary Number of timestamps to retrieve
 * @param primary_vsync Array to store primary timestamps
 * @param roundtrip_us Output parameter (always 0 for local access)
 * @param primary_pipe Primary pipe ID to get vsyncs from
 * @return 0 on success, 1 on failure
 */
static int capture_primary_vsync_local(
	int timestamps_from_primary,
	uint64_t *primary_vsync,
	uint64_t *roundtrip_us,
	int primary_pipe)
{
	*roundtrip_us = 0; // No network delay in local mode

	if (!get_last_vsyncs(primary_vsync, timestamps_from_primary)) {
		ERR("Not enough primary vsync data available yet from pipe %d", primary_pipe);
		return 1;
	}

	print_vsyncs((char*)"PRIMARY", primary_vsync, timestamps_from_primary);
	return 0;
}

/**
 * @brief
 * Unified primary capture function (dispatches to network or local)
 *
 * @param timestamps_from_primary Number of timestamps to request from primary
 * @param primary_vsync Array to store primary timestamps
 * @param roundtrip_us Output parameter for round-trip time in microseconds
 * @param primary_pipe Primary pipe ID (-1 for network mode, >=0 for local mode)
 * @param server_ip Server IP address (for network mode)
 * @param eth_addr Ethernet address for PTP (NULL for TCP, for network mode)
 * @return 0 on success, 1 on failure
 */
static int capture_primary_vsync_once(
	int timestamps_from_primary,
	uint64_t *primary_vsync,
	uint64_t *roundtrip_us,
	int primary_pipe,
	const char *server_ip,
	const char *eth_addr)
{
	if (primary_pipe >= 0) {
		return capture_primary_vsync_local(timestamps_from_primary, primary_vsync, roundtrip_us, primary_pipe);
	} else {
		return capture_primary_vsync_network(timestamps_from_primary, primary_vsync, roundtrip_us, server_ip, eth_addr);
	}
}

/**
 * @brief
 * Capture SECONDARY vsync for a given pipe
 *
 * @param pipe Pipe ID
 * @param timestamps Number of timestamps to capture
 * @param secondary_vsync Array to store secondary timestamps
 * @return 0 on success, 1 on failure
 */
static int capture_secondary_vsync_one(
	int pipe, int timestamps, uint64_t *secondary_vsync)
{
	if (timestamps > VSYNC_MAX_TIMESTAMPS) {
		ERR("Too many timestamps (max %d)", VSYNC_MAX_TIMESTAMPS);
		return 1;
	}
	if (get_vsync(g_devicestr, secondary_vsync, timestamps, pipe)) {
		ERR("Failed to capture secondary vsyncs (pipe %d)", pipe);
		return 1;
	}
	print_vsyncs((char*)"SECONDARY", secondary_vsync, timestamps);
	return 0;
}

/**
 * @brief
 * Unified pipe synchronization function with configurable primary source
 * Handles a SINGLE pipe synchronization (no threading at this level)
 *
 * @param server_ip Server IP (NULL for local mode)
 * @param eth_addr Ethernet address for PTP (NULL for TCP or local)
 * @param pipe Single pipe ID to synchronize
 * @param sync_threshold_us Synchronization threshold
 * @param timestamps Number of timestamps to collect
 * @param shift PLL shift value
 * @param time_period Time period for learning
 * @param learning_rate Learning rate
 * @param shift2 Secondary shift value
 * @param overshoot_ratio Overshoot ratio
 * @param step_threshold Step threshold
 * @param wait_between_steps Wait between steps
 * @param primary_pipe Primary pipe ID (-1 for network mode, >=0 for local mode)
 * @return 0 on success, 1 on failure
 */
int do_pipe_sync(
	const char *server_ip,
	const char *eth_addr,
	int pipe,
	int sync_threshold_us,
	int timestamps,
	double shift,
	int time_period,
	double learning_rate,
	double shift2,
	double overshoot_ratio,
	int step_threshold,
	int wait_between_steps,
	int primary_pipe)
{
	const int N_EXTRA = 5;

	// Ensure we maintain N_EXTRA buffer for network delay compensation
	// If requesting timestamps + N_EXTRA would exceed VSYNC_MAX_TIMESTAMPS,
	// reduce secondary count to preserve the extra primary timestamps
	int actual_timestamps = timestamps;
	int timestamps_from_primary = timestamps + N_EXTRA;

	if (timestamps_from_primary > VSYNC_MAX_TIMESTAMPS) {
		timestamps_from_primary = VSYNC_MAX_TIMESTAMPS;
		actual_timestamps = VSYNC_MAX_TIMESTAMPS - N_EXTRA;
		DBG("Reducing secondary timestamps to %d (primary: %d) to maintain N_EXTRA buffer\n",
			 actual_timestamps, timestamps_from_primary);
	}

	// Buffers for this single pipe
	uint64_t secondary_vsync[VSYNC_MAX_TIMESTAMPS];
	uint64_t primary_vsync[VSYNC_MAX_TIMESTAMPS + 16];
	uint64_t matching_timestamp = 0;
	uint64_t primary_rtt_us = 0;

	// Load persisted counters for this pipe
	PipeStats stats{};
	get_pipe_stats(pipe, &stats);

	// Capture SECONDARY timestamps
	if (capture_secondary_vsync_one(pipe, actual_timestamps, secondary_vsync)) {
		return 1;
	}

	// Capture PRIMARY timestamps (local if primary_pipe >= 0, network otherwise)
	if (capture_primary_vsync_once(timestamps_from_primary,
									primary_vsync,
									&primary_rtt_us,
									primary_pipe,
									server_ip,
									eth_addr)) {
		return 1;
	}

	// Compute delta vs primary
	long delta = find_minimum_delta(primary_vsync,
									secondary_vsync,
									actual_timestamps,
									timestamps_from_primary,
									&matching_timestamp);

	const long CLOCK_SYNC_TOLERANCE_US = 35 * 1000; // 35 ms
	if (llabs(delta) > CLOCK_SYNC_TOLERANCE_US) {
		ERR("Clocks not synchronized (pipe %d). Delta: %ld us", pipe, llabs(delta));
		return 1;
	}

	long avg_primary = find_avg(primary_vsync, timestamps_from_primary);
	long avg_secondary = find_avg(secondary_vsync, actual_timestamps);

	DBG("Pipe %d: avg primary = %ld us, avg secondary = %ld us\n",
		pipe, avg_primary, avg_secondary);
	DBG("Pipe %d: Primary RTT ~%.3f ms\n", pipe, primary_rtt_us / US_IN_MS);

	delta = normalize_delta(delta, avg_secondary);

	// Send sync status to primary's monitoring port if enabled
	if (g_enable_monitoring && server_ip) {
		char hostname[MAX_HOSTNAME_LEN];
		gethostname(hostname, MAX_HOSTNAME_LEN - 1);
		hostname[MAX_HOSTNAME_LEN - 1] = '\0';

		// Calculate monitor port from primary sync port (default 5000)
		int monitor_port = 5000 + MONITOR_PORT_OFFSET;
		send_sync_status(server_ip, monitor_port, hostname, pipe, delta);
	}

	// Per-pipe decision & sync
	os_timespec now;
	os_clock_gettime(OS_CLOCK_MONOTONIC, &now);
	int64_t duration = timespec_to_ms(&now) - timespec_to_ms(&stats.last_sync);

	INFO("Delta: %4ld us [%.3f sec since last sync]\n",
			delta, duration / US_IN_MS);

	if (sync_threshold_us && std::abs(delta) > sync_threshold_us) {
		stats.out_of_sync++;

		if (stats.out_of_sync < 2 && stats.sync_count > 1) {
			// Update stats before returning
			set_pipe_stats(pipe, &stats);
			return 0;
		}
		stats.out_of_sync = 0;

		os_clock_gettime(OS_CLOCK_MONOTONIC, &now);
		duration = timespec_to_ms(&now) - timespec_to_ms(&stats.last_sync);
		stats.last_sync = now;

		double delta_ms = (delta * -1.0) / US_IN_MS;
		double current_freq = get_pll_clock(pipe);
		log_to_file_pipe(pipe, false, ",%7.3f,%3ld,%.3f", duration/US_IN_MS, delta, current_freq);
		INFO("Synchronizing after %.3f seconds.\n", duration/US_IN_MS);

		int sret = perform_synchronization(
			delta_ms,
			pipe,
			shift,
			shift2,
			step_threshold,
			wait_between_steps,
			learning_rate,
			time_period,
			overshoot_ratio,
			duration,
			stats.sync_count,
			stats.success_iter);

		if (sret) {
			set_pipe_stats(pipe, &stats);
			return 1;
		}

		os_clock_gettime(OS_CLOCK_MONOTONIC, &stats.last_sync);
		stats.success_iter = 0;
		stats.sync_count++;
	} else {
		stats.out_of_sync = 0;
		stats.success_iter++;
	}

	// Persist updated counters
	set_pipe_stats(pipe, &stats);
	return 0;
}
