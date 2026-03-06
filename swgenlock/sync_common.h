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

#ifndef _SYNC_COMMON_H
#define _SYNC_COMMON_H

// C++ standard headers (alphabetically)
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <stdarg.h>
#include <climits>
#include <signal.h>
#include <cmath>
#include <regex>
#include <set>

// POSIX/System headers
#include <arpa/inet.h>
#include <getopt.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>

// Project headers
#include "vsyncalter.h"
#include "process_platform.h"
#include "system_platform.h"
#include "connection.h"
#include "message.h"

using namespace std;

extern connection *server;
extern int client_done;

#define MAX_DEVICE_NAME_LENGTH 64
extern char g_devicestr[MAX_DEVICE_NAME_LENGTH];

void server_close_signal(int sig);
void client_close_signal(int sig);

int do_primary(const char *ptp_if, int pipe);

// Unified pipe synchronization function (network or local mode)
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
	int primary_pipe = -1);

// Primary vsync collector interface (for pipelock mode)
void start_vsync_collector(const char* device_str, int pipe, size_t max_count);
void stop_vsync_collector();
bool get_last_vsyncs(uint64_t* dest, size_t count);

void print_vsyncs(char *msg, uint64_t *va, int sz);
void init_log_file_name(int pipe);
void log_to_file(bool comment, const char *fmt, ...);
void init_log_file_name_for_pipe(int pipe);
void init_log_files_from_sentinel(const int *pipes);
void log_to_file_pipe(int pipe, bool comment, const char *fmt, ...);
long find_avg(const uint64_t *va, int sz);
bool is_valid_ip4(const std::string &ip);

// Function to convert timespec structure to milliseconds
long long timespec_to_ms(os_timespec *ts);
int is_not_zero(double value);
uint64_t get_realtime_timestamp_us(void);

// --- Per-pipe persistent stats ---
typedef struct PipeStats {
	u_int32_t success_iter;
	u_int32_t sync_count;
	u_int32_t out_of_sync;
	os_timespec last_sync;      // Per-pipe last sync timestamp
	int thread_continue;        // Per-pipe thread continue flag (for background_task)
} PipeStats;

void get_pipe_stats(int pipe, PipeStats* out);       // thread-safe
void set_pipe_stats(int pipe, const PipeStats* in);  // thread-safe
void reset_pipe_stats(int pipe);

// --- Sync status monitoring (for primary) ---
#define MONITOR_PORT_OFFSET 1000  // Monitoring port = primary port + 1000

void start_sync_monitor_thread(const char* primary_ip, int primary_port);
void stop_sync_monitor_thread();
void send_sync_status(const char* primary_ip, int monitor_port,
					  const char* hostname, int pipe, long delta_us);

extern bool g_enable_monitoring;

#endif // _SYNC_COMMON_H
