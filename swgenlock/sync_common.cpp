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
#include "shared_macros.h"

connection *server;
int client_done = 0;
char g_devicestr[MAX_DEVICE_NAME_LENGTH];
bool g_enable_monitoring = false;

/**
* @brief
* This function closes the server's socket
* @param sig - The signal that was received
* @return void
*/
void server_close_signal(int sig UNUSED)
{
	DBG("Closing server's socket\n");
	stop_sync_monitor_thread();
	server->close_server();
	delete server;
	server = nullptr;
	vsync_lib_uninit();
	exit(1);
}

/**
* @brief
* This function closes the client's socket
* @param sig - The signal that was received
* @return void
*/
void client_close_signal(int sig UNUSED)
{
	DBG("Closing client's socket\n");

	// Inform vsync lib about Ctrl+C signal
	shutdown_lib();

	client_done = 1;
}

/**
* @brief
* This function prints out the last N vsyncs that the system has
* received either on its own or from another system. It will only print in DBG
* mnode
* @param *msg - Any prefix message to be printed.
* @param *va - The array of vsyncs
* @param sz - The size of this array
* @return void
*/
void print_vsyncs(char *msg, uint64_t *va, int sz)
{
	char buffer[80];

	DBG("%s VSYNCS\n", msg);
	for(int i = 0; i < sz; i++) {
		uint64_t microseconds_since_epoch = va[i];
		time_t seconds_since_epoch = microseconds_since_epoch / 1000000;

		// Convert to local time
		struct tm *local_time = localtime(&seconds_since_epoch);
		// Prepare buffer for formatted date/time

		// Format the local time according to the system's locale
		strftime(buffer, sizeof(buffer), "%x %X", local_time);  // %x for date, %X for time

		// Print formatted date/time and microseconds
		DBG("Received VBlank time stamp [%2d]: %llu -> %s [+%-6u us] \n", i,
			microseconds_since_epoch,
			buffer,
			static_cast<unsigned int>(microseconds_since_epoch % 1000000));
	}
}

#define LOG_PREFIX "sync_pipe_"
#define LOG_SUFFIX ".csv"
char log_filename[128];  // Holds the log file name with date and time
os_timespec log_start_time;
// Call this once at program start
void init_log_file_name(int pipe) {
	time_t now = time(nullptr);
	struct tm *tm_info = localtime(&now);
	char datetime[32];

	// Format: YYYY-MM-DD_HH-MM-SS
	strftime(datetime, sizeof(datetime), "%Y%m%d_%H%M%S", tm_info);

	// Generate filename: synchronization_pipe_<pipe>_<datetime>.log
	snprintf(log_filename, sizeof(log_filename), "%s%d_%s%s",
			LOG_PREFIX, pipe, datetime, LOG_SUFFIX);
	os_clock_gettime(OS_CLOCK_REALTIME, &log_start_time);  // Save start time
}

// log_to_file with printf-style formatting
void log_to_file(bool comment, const char *fmt, ...) {
	FILE *fp = fopen(log_filename, "a");
	if (!fp) {
		perror("Failed to open log file");
		return;
	}

	os_timespec now;
	os_clock_gettime(OS_CLOCK_REALTIME, &now);

	long delta_sec = now.tv_sec - log_start_time.tv_sec;
	long delta_nsec = now.tv_nsec - log_start_time.tv_nsec;
	if (delta_nsec < 0) {
		delta_sec--;
		delta_nsec += 1000000000;
	}

	double delta = delta_sec + (delta_nsec / 1e9);

	// Format the message
	char message[2048];
	va_list args;
	va_start(args, fmt);
	vsnprintf(message, sizeof(message), fmt, args);
	va_end(args);

	// Log format: [+1.234s] Your message
	fprintf(fp, "%s[+%.3fs] %s\n", comment ? "#": "", delta, message);
	fclose(fp);
}

#define LOG_PREFIX "sync_pipe_"
#define LOG_SUFFIX ".csv"
static const int MAX_PIPES = 4;

static char            g_log_filenames[MAX_PIPES][128];
static os_timespec     g_log_start_time[MAX_PIPES];
static pthread_mutex_t g_log_mutex[MAX_PIPES] = {
	PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER
};
static bool            g_log_inited[MAX_PIPES] = {false, false, false, false};

// init one pipe’s log file name (safe to call multiple times)
void init_log_file_name_for_pipe(int pipe) {
	if (pipe < 0 || pipe >= MAX_PIPES) return;

	if (!g_log_inited[pipe]) {
		time_t now = time(nullptr);
		struct tm *tm_info = localtime(&now);
		char datetime[32];
		strftime(datetime, sizeof(datetime), "%Y%m%d_%H%M%S", tm_info);

		snprintf(g_log_filenames[pipe], sizeof(g_log_filenames[pipe]),
				 "%s%d_%s%s", LOG_PREFIX, pipe, datetime, LOG_SUFFIX);

		os_clock_gettime(OS_CLOCK_REALTIME, &g_log_start_time[pipe]);
		g_log_inited[pipe] = true;
	}
}

// optional helper if you pass a sentinel list like {0,2,3,-1}
void init_log_files_from_sentinel(const int *pipes) {
	if (!pipes) return;
	for (const int *p = pipes; *p != -1; ++p) {
		init_log_file_name_for_pipe(*p);
	}
}

// per-pipe logger (CSV/printf-style)
void log_to_file_pipe(int pipe, bool comment, const char *fmt, ...) {
	if (pipe < 0 || pipe >= MAX_PIPES) return;
	if (!g_log_inited[pipe]) init_log_file_name_for_pipe(pipe);

	pthread_mutex_lock(&g_log_mutex[pipe]);

	FILE *fp = fopen(g_log_filenames[pipe], "a");
	if (!fp) {
		perror("Failed to open per-pipe log file");
		pthread_mutex_unlock(&g_log_mutex[pipe]);
		return;
	}

	os_timespec now;
	os_clock_gettime(OS_CLOCK_REALTIME, &now);

	long delta_sec  = now.tv_sec  - g_log_start_time[pipe].tv_sec;
	long delta_nsec = now.tv_nsec - g_log_start_time[pipe].tv_nsec;
	if (delta_nsec < 0) { delta_sec--; delta_nsec += 1000000000; }
	double delta = delta_sec + (delta_nsec / 1e9);

	char message[2048];
	va_list args;
	va_start(args, fmt);
	vsnprintf(message, sizeof(message), fmt, args);
	va_end(args);

	// comment lines start with '#', data lines without
	fprintf(fp, "%s[+%.3fs] %s\n", comment ? "#" : "", delta, message);
	fclose(fp);

	pthread_mutex_unlock(&g_log_mutex[pipe]);
}

/**
* @brief
* This function finds the average of all the vertical syncs that
* have been provided by the primary system to the secondary.
* @param *va - The array holding all the vertical syncs of the primary system
* @param sz - The size of this array
* @return The average of the vertical syncs
*/
long find_avg(const uint64_t *va, int sz)
{
	int avg = 0;
	for(int i = 0; i < sz - 1; i++) {
		avg += va[i+1] - va[i];
	}
	return avg / ((sz == 1) ? sz : (sz - 1));
}

bool is_valid_ip4(const std::string &ip)
{
	try {
			static const std::regex ipv4_pattern(
				R"((^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]
					|2[0-4][0-9]|[01]?[0-9][0-9]?)$))");
			return std::regex_match(ip, ipv4_pattern);
	}
	catch (const std::regex_error &e) {
			// Log or handle the regex error
			return false;
	}
	catch (...) {
			// Handle other unforeseen exceptions
			return false;
	}
}

// Function to convert timespec structure to milliseconds
long long timespec_to_ms(os_timespec *ts) {
	return (ts->tv_sec * 1000LL) + (ts->tv_nsec / 1000000LL);
}

int is_not_zero(double value) {
	const double EPSILON = 1e-9;   // Small threshold to check if delta is zero
	return fabs(value) >= EPSILON;
}

/**
 * @brief
 * Get current realtime clock timestamp since Unix epoch (1970-01-01 00:00:00 UTC)
 * with microsecond accuracy
 *
 * @return uint64_t - Timestamp in microseconds since epoch, or 0 on error
 */
uint64_t get_realtime_timestamp_us(void)
{
	os_timespec ts;

	if (os_clock_gettime(OS_CLOCK_REALTIME, &ts) != 0) {
		ERR("Failed to get CLOCK_REALTIME: %s\n", strerror(errno));
		return 0;
	}

	// Convert to microseconds: seconds * 1,000,000 + nanoseconds / 1,000
	uint64_t timestamp_us = (uint64_t)ts.tv_sec * 1000000ULL +
							(uint64_t)(ts.tv_nsec / 1000ULL);

	return timestamp_us;
}

// ---------------- Per-pipe persistent stats (0..3) ----------------
static PipeStats g_pipe_stats[4] = {
	{0,0,0,{0,0},1},{0,0,0,{0,0},1},{0,0,0,{0,0},1},{0,0,0,{0,0},1}
};
static pthread_mutex_t g_pipe_stats_mtx[4] = {
	PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER
};

void get_pipe_stats(int pipe, PipeStats* out)
{
	if (!out || pipe < 0 || pipe > 3) return;
	pthread_mutex_lock(&g_pipe_stats_mtx[pipe]);
	*out = g_pipe_stats[pipe];
	pthread_mutex_unlock(&g_pipe_stats_mtx[pipe]);
}

void set_pipe_stats(int pipe, const PipeStats* in)
{
	if (!in || pipe < 0 || pipe > 3) return;
	pthread_mutex_lock(&g_pipe_stats_mtx[pipe]);
	g_pipe_stats[pipe] = *in;
	pthread_mutex_unlock(&g_pipe_stats_mtx[pipe]);
}

void reset_pipe_stats(int pipe)
{
	if (pipe < 0 || pipe > 3) return;
	pthread_mutex_lock(&g_pipe_stats_mtx[pipe]);
	g_pipe_stats[pipe] = {0,0,0,{0,0},1};
	os_clock_gettime(OS_CLOCK_MONOTONIC, &g_pipe_stats[pipe].last_sync);
	pthread_mutex_unlock(&g_pipe_stats_mtx[pipe]);
}

/**
 * @brief
 * Send sync status update from secondary to primary's monitoring port
 * Uses UDP for fire-and-forget delivery (no blocking on secondary side)
 *
 * @param primary_ip IP address of the primary
 * @param monitor_port Port number for monitoring (typically primary_port + MONITOR_PORT_OFFSET)
 * @param hostname Name of this secondary node
 * @param pipe Pipe number being synchronized
 * @param delta_us Delta in microseconds (positive or negative)
 */
void send_sync_status(const char* primary_ip, int monitor_port,
					  const char* hostname, int pipe, long delta_us)
{
	if (!g_enable_monitoring || !primary_ip || !hostname) {
		return;
	}

	// Create UDP socket for sending (fire and forget)
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		DBG("Failed to create socket for sync status: %s\n", strerror(errno));
		return;
	}

	// Prepare sync status message
	sync_status_msg status_msg;
	status_msg.set_sync_status(hostname, pipe, delta_us, get_realtime_timestamp_us());

	// Setup destination address
	struct sockaddr_in dest_addr;
	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(monitor_port);
	dest_addr.sin_addr.s_addr = inet_addr(primary_ip);

	// Send the message (fire and forget, best effort)
	ssize_t sent = sendto(sockfd, &status_msg, sizeof(status_msg), 0,
		   (struct sockaddr*)&dest_addr, sizeof(dest_addr));
	if (sent < 0) {
		DBG("Failed to send sync status to primary: %s\n", strerror(errno));
	}

	close(sockfd);
}
