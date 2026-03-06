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

// C++ standard headers
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <cmath>

// POSIX/System headers
#include <getopt.h>
#include <pthread.h>
#include <signal.h>

// Project headers
#include "vsyncalter.h"
#include "process_platform.h"
#include "system_platform.h"

static volatile bool thread_continue = true;

constexpr int MAX_DEVICE_NAME_LENGTH = 64;
constexpr double MICROSECONDS_TO_MILLISECONDS = 1000.0;
static char g_devicestr[MAX_DEVICE_NAME_LENGTH];

/**
* @brief
* This function informs vsynclib about termination.
* @param sig - The signal that was received
* @return void
*/
static void terminate_signal(int )
{
	thread_continue = false;

	// Inform vsync lib about Ctrl+C signal
	shutdown_lib();
	INFO("Terminating due to Ctrl+C...\n");
}

/**
 * @brief
 * This function is the background thread task to call print_vblank_interval
 *
 * @param arg
 * @return void*
 */
static void* background_task(void* arg)
{
	const int WAIT_TIME_IN_MICRO = 1000 * 1000;  // 1 sec
	double avg_interval=0.0;
	const int pipe = *static_cast<int*>(arg);
	os_timespec start_time = {0, 0};
	os_timespec current_time = {0, 0};

	INFO("VBlank interval during synchronization ->\n");
	os_clock_gettime(OS_CLOCK_MONOTONIC, &start_time);
	// Thread runs this loop until it is canceled
	while (thread_continue) {
		os_usleep(WAIT_TIME_IN_MICRO);
		// Calculate elapsed time
		os_clock_gettime(OS_CLOCK_MONOTONIC, &current_time);
		const double elapsed_time = (current_time.tv_sec - start_time.tv_sec) +
			((current_time.tv_nsec - start_time.tv_nsec) / 1e9);

		avg_interval = get_vblank_interval(g_devicestr, pipe, 30);
		INFO("\t[Elapsed Time: %.2lf s] Time average of the vsyncs on pipe %d is %.6lf ms\n", elapsed_time, pipe, avg_interval);
	}
	return nullptr;
}

/**
 * @brief
 * Print help message
 *
 * @param program_name - Name of the program
 * @return void
 */
void print_help(const char *program_name)
{
	// Using printf for printing help
	printf("Usage: %s [-p pipe] [-d delta] [-s shift] [-v loglevel] ... [-h]\n"
		"Options:\n"
		"  -p pipe            Pipe to get stamps for.  0,1,2 ... (default: %d)\n"
		"  -d delta           Drift time in us to achieve (default: 1000 us) e.g 1000 us = 1.0 ms\n"
		"  -s shift           PLL frequency change fraction (default: %.2f)\n"
		"  -x shift2          PLL frequency change fraction for large drift (default: %.2f)\n"
		"  -e device          Device string (default: %s)\n"
		"  -f frequency       Clock value to directly set (default -> Do not set : 0.0) \n"
		"  -v loglevel        Log level: error, warning, info, debug or trace (default: info)\n"
		"  -t step_threshold  Delta threshold in microseconds to trigger stepping mode (default: 1000 us)\n"
		"  -w step_wait       Wait in milliseconds between steps (default: 50 ms) \n"
		"  -R refresh_time    Set PLL frequency to achieve desired interval in micro seconds (default: disabled). e.g 16666.666 us\n"
		"  -r or --no-reset   Do no reset to original values. Keep modified PLL frequency and exit (default: reset)\n"
		"  -c or --no-commit  Do no commit changes.  Just print (default: commit)\n"
		"  -H or --hh         Enable hardware timestamping (default: disabled)\n"
		"  -m or --mn         Use DP M & N Path. (default: no)\n"
		"  -h                 Display this help message\n",
		program_name, VSYNC_DEFAULT_PIPE, VSYNC_DEFAULT_SHIFT, VSYNC_DEFAULT_SHIFT2, find_first_dri_card());
}

/**
* @brief
* This is the main function
* @param argc - The number of command line arguments
* @param *argv[] - Each command line argument in an array
* @return
* - 0 = SUCCESS
* - 1 = FAILURE
*/
int main(int argc, char *argv[])
{
	// Threading variables
	pthread_t tid = 0;
	int status = 0;

	// Configuration variables
	std::string device_str = find_first_dri_card();
	std::string log_level = "info";
	int pipe = VSYNC_DEFAULT_PIPE;  // Default pipe# 0
	int delta = 1000;  // Drift in us to achieve from current time
	double shift = VSYNC_DEFAULT_SHIFT, shift2 = VSYNC_DEFAULT_SHIFT2;
	double frequency = 0.0, refresh_period = 0.0, avg_interval = 0.0;
	bool reset = true, commit = true, m_n = false, hardware_ts = false;
	int step_threshold = VSYNC_TIME_DELTA_FOR_STEP, wait_between_steps = VSYNC_DEFAULT_WAIT_IN_MS;

	// Command-line parsing
	static struct option long_options[] = {
		{"no-reset", no_argument, nullptr, 'r'},
		{"no-commit", no_argument, nullptr, 'c'},
		{"mn", no_argument, nullptr, 'm'},
		{"hh", no_argument, nullptr, 'H'},
		{0, 0, 0, 0}
	};
	int opt = 0;

	printf("PLL Control Version: %s\n", get_version().c_str());

	while ((opt = getopt_long(argc, argv, "p:d:s:x:e:n:f:t:w:R:v:hmrcH", long_options, nullptr)) != -1) {
		switch (opt) {
			case 'p':
				pipe = std::stoi(optarg);
				break;
			case 'd':
				delta = std::stoi(optarg);
				break;
			case 's':
				shift = std::stod(optarg, nullptr);
				break;
			case 'x':
				shift2 = std::stod(optarg, nullptr);
				break;
			case 'e':
				device_str=optarg;
				break;
			case 'r':
				reset = false;
				break;
			case 'c':
				commit = false;
				break;
			case 'm':
				m_n = true;
				break;
			case 't':
				step_threshold = std::stoi(optarg, nullptr);
				break;
			case 'w':
				wait_between_steps = std::stoi(optarg, nullptr);
				break;
			case 'R':
				refresh_period = std::stod(optarg, nullptr);
				break;
			case 'v':
				log_level = optarg;
				set_log_level_str(optarg);
				break;
			case 'f':
				frequency = std::stod(optarg, nullptr);
				break;
			case 'H':
				hardware_ts = true;
				break;
			case 'h':
				print_help(argv[0]);
				exit(EXIT_SUCCESS);
			case '?':
				print_help(argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	// Print configurations
	INFO("Configuration:\n");
	INFO("\tPipe ID: %d\n", pipe);
	INFO("\tDevice: %s\n", device_str.c_str());
	INFO("\tDelta: %d microseconds\n", delta);
	INFO("\tShift: %lf\n", shift);
	INFO("\tFrequency to set: %lf\n", frequency);
	INFO("\tLog Level: %s\n", log_level.c_str());
	INFO("\tstep_threshold: %d us\n", step_threshold);
	INFO("\tHardware Timestamping: %s\n", hardware_ts ? "Enabled" : "Disabled");
	INFO("\twait_between_steps: %d ms\n", wait_between_steps);

	memset(g_devicestr,0, MAX_DEVICE_NAME_LENGTH);
	// Copy until src string size or max size - 1.
	strncpy(g_devicestr, device_str.c_str(), MAX_DEVICE_NAME_LENGTH - 1);

	if(vsync_lib_init(device_str.c_str(), m_n, static_cast<bool>(hardware_ts))) {
		ERR("Failed to initialize vsync library with device: %s\n", device_str.c_str());
		return 1;
	}

	struct sigaction sig_int_handler;
	sig_int_handler.sa_handler = terminate_signal;
	sigemptyset(&sig_int_handler.sa_mask);
	sig_int_handler.sa_flags = 0;
	sigaction(SIGINT, &sig_int_handler, nullptr);
	sigaction(SIGTERM, &sig_int_handler, nullptr);

	if (commit) {
		// synchronize_vsync function is synchronous call and does not
		// output any information. To enhance visibility, a thread is
		// created for logging vblank intervals while synchronization is
		// in progress.
		status = pthread_create(&tid, nullptr, background_task, &pipe);
		if (status != 0) {
			ERR("Cannot create thread");
			return 1;
		}
	}

	// If frequency is set, directly set the PLL clock
	if (frequency > 0.0) {

		// shift is used internally to create steps if delta between
		// current and desired frequency is large
		set_pll_clock(frequency, pipe, shift, wait_between_steps);

	} else if (refresh_period > 0.0) {  // Set PLL clock to a specific refresh period.

		// Iterative PLL adjustment algorithm to match desired refresh period
		// This implements a closed-loop control system that:
		// 1. Measures current vblank interval (actual refresh period)
		// 2. Compares it with the desired refresh period
		// 3. Makes small PLL frequency adjustments (0.001%) in the correct direction
		// 4. Repeats until convergence within tolerance (100ns) or max iterations
		//
		// PLL frequency relationship: Higher PLL freq = shorter vblank interval (faster refresh)
		//                            Lower PLL freq = longer vblank interval (slower refresh)

		double pll_clock  = get_pll_clock(pipe);
		double vblank_interval = get_vblank_interval(g_devicestr, pipe, VSYNC_MAX_TIMESTAMPS) * 1000.0;

		const double TOLERANCE = 0.1; // 100 ns tolerance
		const double SMALL_INCREMENT = 0.001; // 0.001% increment
		int iteration = 0;
		const int MAX_ITERATIONS = 1000; // Safety limit

		INFO("Starting iterative PLL adjustment...\n");
		INFO("Initial - VBlank interval: %.3f us, Desired: %.3f us, PLL clock: %.3f Hz\n",
				vblank_interval, refresh_period, pll_clock);

		while (fabs(vblank_interval - refresh_period) > TOLERANCE && iteration < MAX_ITERATIONS && thread_continue) {
			iteration++;

			// Calculate percentage change needed
			double difference = refresh_period - vblank_interval;
			double percentage_change = SMALL_INCREMENT;

			// If vblank_interval > refresh_period, we need to slow down (decrease PLL)
			// If vblank_interval < refresh_period, we need to speed up (increase PLL)
			if (difference > 0) {
				// Need to slow down - decrease PLL frequency
				pll_clock *= (1.0 - (percentage_change / 100.0));
			} else {
				// Need to speed up - increase PLL frequency
				pll_clock *= (1.0 + (percentage_change / 100.0));
			}

		// Set new PLL clock
		set_pll_clock(pll_clock, pipe, shift, wait_between_steps);

		// Wait a bit for the change to take effect
		os_usleep(20000); // 20ms

		// Measure new vblank interval
		vblank_interval = get_vblank_interval(g_devicestr, pipe, VSYNC_MAX_TIMESTAMPS) * 1000.0;			INFO("Iteration %d - VBlank: %.3f us, Target: %.3f us, Diff: %.3f us, PLL: %.3f Hz\n",
					iteration, vblank_interval, refresh_period, difference, pll_clock);
		}

		if (iteration >= MAX_ITERATIONS) {
			WARNING("Reached maximum iterations (%d) without converging\n", MAX_ITERATIONS);
		} else {
			INFO("Converged after %d iterations. Final VBlank interval: %.3f us\n",
					iteration, vblank_interval);
		}
	} else {

		// Convert delta to milliseconds before calling
		synchronize_vsync((double) delta / MICROSECONDS_TO_MILLISECONDS, pipe, shift, shift2, step_threshold, wait_between_steps, reset, commit);

	}

	// Set flag to false to signal the thread to terminate
	thread_continue = false;

	if (commit) {
		// Wait for the thread to terminate
		pthread_join(tid, nullptr);
		avg_interval = get_vblank_interval(static_cast<const char*>(g_devicestr), pipe, VSYNC_MAX_TIMESTAMPS);
		INFO("VBlank interval after synchronization ends: %lf ms\n", avg_interval);
	}

	vsync_lib_uninit();
	return 0;
}
