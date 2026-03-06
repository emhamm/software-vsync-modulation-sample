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

// Unified context for pipe synchronization thread (network or local mode)
struct PipeSyncThreadCtx {
	std::string server_ip;      // Empty for pipelock mode
	std::string mac_address;    // Empty for TCP or pipelock mode
	int primary_pipe;           // -1 for network mode, >=0 for pipelock mode
	int pipe;                   // Secondary pipe to sync
	int delta;
	int timestamps;
	double shift;
	int time_period;
	double learning_rate;
	double shift2;
	double overshoot_ratio;
	int step_threshold;
	int wait_between_steps;
};

/**
 * @brief Unified thread function for pipe synchronization (network or pipelock mode)
 *
 * Handles both secondary mode (network) and pipelock mode (local) based on primary_pipe value:
 * - primary_pipe == -1: Network mode (uses do_secondary)
 * - primary_pipe >= 0:  Pipelock mode (uses do_pipelock)
 */
static void* pipe_sync_thread(void *arg)
{
	PipeSyncThreadCtx *ctx = static_cast<PipeSyncThreadCtx*>(arg);

	// Set thread-local pipe context for automatic logging identification
	set_thread_pipe_id(ctx->pipe);

	int ret = 0;
	int timestamps = ctx->timestamps;

	// Keep doing synchronization until Ctrl+C
	try {
		do {
			ret = do_pipe_sync(
				ctx->server_ip.empty() ? nullptr : ctx->server_ip.c_str(),
				ctx->mac_address.empty() ? nullptr : ctx->mac_address.c_str(),
				ctx->pipe,
				ctx->delta,
				timestamps,
				ctx->shift,
				ctx->time_period,
				ctx->learning_rate,
				ctx->shift2,
				ctx->overshoot_ratio,
				ctx->step_threshold,
				ctx->wait_between_steps,
				ctx->primary_pipe);

			timestamps = 2;
			os_sleep_ms(1000);
		} while (!client_done && !ret);
	} catch (const std::exception& e) {
				ERR("Exception during thread join for pipe %d: %s\n", ctx->pipe, e.what());
			} catch (...) {
				ERR("Unknown exception during thread join for pipe %d\n", ctx->pipe);
			}
	return nullptr;
}

static std::string join_ints(const std::vector<int>& v) {
	std::ostringstream oss;
	for (size_t i = 0; i < v.size(); ++i) {
		if (i) oss << ",";
		oss << v[i];
	}
	return oss.str();
}

static bool parse_pipes_arg(const std::string& arg, std::vector<int>& out) {
	// Accept: "4" => all pipes 0..3
	// Or comma list like "0,2,3"
	std::set<int> uniq;
	auto trim = [](std::string s) {
		auto issp = [](int c){ return c==' ' || c=='\t' || c=='\n' || c=='\r'; };
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), [&](int ch){ return !issp(ch); }));
		s.erase(std::find_if(s.rbegin(), s.rend(), [&](int ch){ return !issp(ch); }).base(), s.end());
		return s;
	};

	std::string s = trim(arg);
	if (s.empty()) return false;

	// special case: 4 == all pipes
	if (s == "4") {
		out = {0,1,2,3};
		return true;
	}

	std::stringstream ss(s);
	std::string tok;
	while (std::getline(ss, tok, ',')) {
		tok = trim(tok);
		if (tok.empty()) continue;
		char* endp = nullptr;
		long val = std::strtol(tok.c_str(), &endp, 10);
		if (*tok.c_str() == '\0' || (endp && *endp != '\0')) {
			ERR("Invalid pipe token: %s", tok.c_str());
			return false;
		}
		if (val < 0 || val > 3) {
			ERR("Pipe id out of range (0..3 or 4=all): %ld", val);
			return false;
		}
		uniq.insert((int)val);
	}
	if (uniq.empty()) return false;

	out.assign(uniq.begin(), uniq.end());
	std::sort(out.begin(), out.end());
	return true;
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
	printf("Usage: %s [-m mode] [-i interface] [-c mac_address] [-d delta] [-p pipe] [-s shift] [-v loglevel] ... [-h]\n"
		"Options:\n"
		"  -m mode            Mode of operation: pri, sec, pipelock (default: pri)\n"
		"                       pri      - Primary mode (server)\n"
		"                       sec      - Secondary mode (client)\n"
		"                       pipelock - Primary + Secondary in same process (no network)\n"
		"  -i interface       Network interface to listen on (primary) or connect to (secondary) (default: 127.0.0.1)\n"
		"  -c mac_address     MAC address of the network interface to connect to. Applicable to ethernet interface mode only.\n"
		"  -d delta           Drift time in microseconds to allow before pll reprogramming - in microseconds (default: 100 us)\n"
		"  -p pipes           Pipe(s) for secondary. Use 4 for all pipes or comma list like 0,1,2 (default: 0)\n"
		"  -P primary_pipe    Primary pipe for pipelock mode (default: 0)\n"
		"  -s shift           PLL frequency change fraction (default: 0.01)\n"
		"  -x shift2          PLL frequency change fraction for large drift (default: 0.0; Disabled)\n"
		"  -f frequency       PLL clock value to set at start (default -> Do Not Set : 0.0) \n"
		"  -e device          Device string (default: /dev/dri/card0)\n"
		"  -v loglevel        Log level: error, warning, info, debug or trace (default: info)\n"
		"  -k time_period     Time period in seconds during which learning rate will be applied.  (default: 240 sec)\n"
		"  -l learning_rate   Learning rate for convergence. Secondary mode only. e.g 0.00001 (default: 0.0  Disabled) \n"
		"  -o overshoot_ratio Allow the clock to go beyond zero alignment by a ratio of the delta (value between 0 and 1). \n"
		"                        For example, with -o 0.5 and delta=500, the target offset becomes -250 us in the apposite direction (default: 0.0)\n"
		"  -t step_threshold  Delta threshold in microseconds to trigger stepping mode (default: 1000 us)\n"
		"  -w step_wait       Wait in milliseconds between steps (default: 50 ms) \n"
		"  -M                 Enable sync status monitoring (Primary collects status from secondaries to CSV file)\n"
		"  -H                 Enable hardware timestamping (default: disabled)\n"
		"  -n                 Use DP M & N Path. (default: no)\n"
		"  -h                 Display this help message\n",
		program_name);

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
	int ret = 0;

	// Configuration strings
	std::string modeStr = "pri", log_level = "info";
	std::string interface_or_ip = "127.0.0.1";  // Default to localhost
	std::string mac_address = "", device_str = find_first_dri_card();

	// Integer configuration
	int delta = 100, timestamps = VSYNC_MAX_TIMESTAMPS;
	int time_period = 480, step_threshold = VSYNC_TIME_DELTA_FOR_STEP, wait_between_steps = VSYNC_DEFAULT_WAIT_IN_MS;

	// Double configuration
	double shift = 0.01, shift2 = 0.0, frequency = 0.0;
	double learning_rate = 0.0001, overshoot_ratio = 0.0;

	// Boolean flags
	bool m_n = false, hardware_ts = false;

	// Pipes (parsed from -p)
	std::vector<int> pipes_vec;            // actual list
	std::string pipes_arg = "0";           // default as string
	int primary_pipe = -1;                  // for pipelock mode (-P)

	// Command-line parsing
	static struct option long_options[] = {
		{"mn", no_argument, nullptr, 'n'},
		{"hh", no_argument, nullptr, 'H'},
		{0, 0, 0, 0}
	};
	int opt, option_index = 0;

	while ((opt = getopt_long(argc, argv, "m:i:c:p:P:d:s:x:f:o:e:k:l:n:t:w:v:hHM", long_options, &option_index)) != -1) {
		switch (opt) {
			case 'm':
				modeStr = optarg;
				break;
			case 'i':
				interface_or_ip = optarg;
				break;
			case 'c':
				mac_address = optarg;
				break;
			case 'p':
				pipes_arg = optarg; // store raw, parse later
				break;
			case 'P':
				primary_pipe = std::stoi(optarg);
				if (primary_pipe < 0 || primary_pipe > 3) {
					ERR("Primary pipe must be 0-3");
					return 1;
				}
				break;
			case 'd':
				delta = std::stoi(optarg);
				break;
			case 's':
				shift = std::stod(optarg);
				break;
			case 'x':
				shift2 = std::stod(optarg);
				break;
			case 'k':
				time_period = std::stoi(optarg);
				break;
			case 'l':
				learning_rate = std::stod(optarg);
				break;
			case 'n':
				m_n = true;
				break;
			case 'f':
				frequency = std::stod(optarg);
				break;
			case 'o':
				overshoot_ratio = std::stod(optarg);
				break;
			case 't':
				step_threshold = std::stoi(optarg);
				break;
			case 'w':
				wait_between_steps = std::stoi(optarg);
				break;
			case 'H':
				hardware_ts = true;
				break;
			case 'M':
				g_enable_monitoring = true;
				break;
			case 'v':
				log_level = optarg;
				set_log_level_str(optarg);
				break;
			case 'e':
				device_str=optarg;
				break;
			case 'h':
				print_help(argv[0]);
				exit(EXIT_SUCCESS);
			case '?':
				print_help(argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	// Parse pipes list
	if (!parse_pipes_arg(pipes_arg, pipes_vec)) {
		ERR("Failed to parse -p argument. Use 4 or comma list e.g. -p 0,1,2");
		return 1;
	}

	if (modeStr == "pri") {
		set_log_mode("[ PRIMARY ]");
	} else if (modeStr == "sec") {
		set_log_mode("[SECONDARY]");
	} else if (modeStr == "pipelock") {
		set_log_mode("[PIPELOCK ]");
	}

	// Print configurations
	INFO("Configuration:\n");
	INFO("\tMode: %s\n", modeStr.c_str());
	INFO("\tInterface or IP: %s\n", interface_or_ip.length() > 0 ? interface_or_ip.c_str() : "N/A");
	INFO("\tSec Mac Address: %s\n", mac_address.length() > 0 ? mac_address.c_str() : "N/A");
	INFO("\tDelta: %d us\n", delta);
	INFO("\tShift: %.3lf\n", shift);
	INFO("\tShift2: %.3lf\n", shift2);
	INFO("\tFrequency: %lf\n", frequency);
	INFO("\tOvershoot Ratio: %lf\n", overshoot_ratio);
	INFO("\tPipes: %s\n", join_ints(pipes_vec).c_str());
	if (modeStr == "pipelock") {
		INFO("\tPrimary Pipe: %d\n", primary_pipe);
	}
	INFO("\tDevice: %s\n", device_str.c_str());
	INFO("\tDP M & N Path: %s\n", m_n ? "Enabled" : "Disabled");
	INFO("\tHardware time stamping: %s\n", hardware_ts ? "Enabled" : "Disabled");
	INFO("\tstep_threshold: %d us\n", step_threshold);
	INFO("\twait_between_steps: %d ms\n", wait_between_steps);
	if (is_not_zero(learning_rate)) {
		INFO("\tLearning rate: %lf\n", learning_rate);
		INFO("\ttime_period: %d sec\n", time_period);
	}
	INFO("\tLog Level: %s\n", log_level.c_str());

	memset(g_devicestr,0, MAX_DEVICE_NAME_LENGTH);
	// Copy until src string size or max size - 1.
	strncpy(g_devicestr, device_str.c_str(), MAX_DEVICE_NAME_LENGTH - 1);

	// Quick kernel patch check: use first pipe only
	if (hardware_ts == false) {
		u_int64_t va[VSYNC_MAX_TIMESTAMPS];
		int test_pipe = pipes_vec.front();
		if (get_vsync(g_devicestr, va, 1, test_pipe) == -1) {
			ERR("Failed to get vsync (pipe %d)\n", test_pipe);
			return 1;
		}

		os_timespec ts;
		if (os_clock_gettime(OS_CLOCK_REALTIME, &ts) == 0) {
			int64_t current_time_in_microseconds =
				((unsigned long long)ts.tv_sec * 1000000ULL) +
				((unsigned long long)ts.tv_nsec / 1000);
			int64_t difference = current_time_in_microseconds - va[0];
			const int DIFF_IN_MICRO = 100000; // 100 ms
			if (labs(difference) > DIFF_IN_MICRO) {
				ERR("DRM kernel patch not applied. Results will be inaccurate\n");
			}
		} else {
			ERR("Failed to get the current time");
			return 1;
		}
	}

	if(vsync_lib_init(g_devicestr, m_n, hardware_ts)) {
		ERR("Failed to initialize vsync library with device: %s\n", g_devicestr);
		return 1;
	}

	if(!modeStr.compare("pri")) {
		// Primary uses one pipe (the server process is set up for a single pipe)
		int pri_pipe = pipes_vec.front();
		ret = do_primary(interface_or_ip.length() > 0 ? interface_or_ip.c_str() : nullptr, pri_pipe);
	} else if(!modeStr.compare("sec") || !modeStr.compare("pipelock")) {
		// Secondary or Pipelock mode: unified handling
		bool is_pipelock = !modeStr.compare("pipelock");

		signal(SIGINT, client_close_signal);
		signal(SIGTERM, client_close_signal);

		// Validate: for pipelock, primary pipe should not be in secondary pipes list
		if (is_pipelock) {
			for (int p : pipes_vec) {
				if (p == primary_pipe) {
					ERR("Primary pipe %d cannot be in secondary pipes list\n", primary_pipe);
					return 1;
				}
			}
		}

		// Per-pipe initial logging and optional PLL set
		for (int p : pipes_vec) {
			char name[32];
			if (!get_phy_name(p, name, sizeof(name))) {
				ERR("Failed to get PHY name for pipe %d\n", p);
				return 1;
			}
			reset_pipe_stats(p);
			init_log_file_name_for_pipe(p);

			// Log mode-specific message
			log_to_file_pipe(p, true, "%s mode starting (pipe %d)",
				is_pipelock ? "Pipelock" : "Secondary", p);

			// Build configuration log (with optional primary pipe for pipelock)
			log_to_file_pipe(p, true,
						"Configuration:\n"
						"#\tTitle: \n"
						"#\tPHY Type: %s\n"
						"#\tPipe ID: %d\n"
						"#\tPrimary Pipe: %d\n"
						"#\tDelta: %d us\n"
						"#\tShift: %.3lf\n"
						"#\tShift2: %.3lf\n"
						"#\tDevice: %s\n"
						"#\tLearning rate: %lf\n"
						"#\ttime_period: %d sec\n"
						"#\tOvershoot ratio: %lf\n"
						"#\tHH: %s",
						name,
						p,
						primary_pipe,
						delta,
						shift,
						shift2,
						device_str.c_str(),
						learning_rate,
						time_period,
						overshoot_ratio,
						hardware_ts ? "Enabled" : "Disabled"
					);

			log_to_file_pipe(p, true, "\n#[Time],duration from last sync (sec),drift delta (us),PLL Frequency");

			if (is_not_zero(frequency)) {
				INFO("Setting PLL clock value to %lf (pipe %d)\n", frequency, p);
				set_pll_clock(frequency, p, shift, wait_between_steps);
			}
		}

		// For pipelock mode, start the primary vsync collector
		if (is_pipelock) {
			const int N_EXTRA = 10;

			if (primary_pipe < VSYNC_PIPE_RANGE_MIN || primary_pipe > VSYNC_PIPE_RANGE_MAX) {
				ERR("Invalid primary pipe value: %d\n", primary_pipe);
				return 1;
			}

			INFO("Starting pipelock mode: Primary pipe %d\n", primary_pipe);
			start_vsync_collector(g_devicestr, primary_pipe, VSYNC_MAX_TIMESTAMPS + N_EXTRA);
			INFO("Waiting for primary vsync buffer to fill...\n");
			os_sleep_ms(1000);
		}		// Create separate thread for each pipe's sync loop

		std::vector<pthread_t> threads(pipes_vec.size());
		std::vector<PipeSyncThreadCtx> contexts(pipes_vec.size());
		std::vector<bool> thread_created(pipes_vec.size(), false);

		for (size_t i = 0; i < pipes_vec.size(); ++i) {
			try {
				if (is_pipelock) {
					// Pipelock mode: local synchronization
					contexts[i].server_ip = "";
					contexts[i].mac_address = "";
					contexts[i].primary_pipe = primary_pipe;
				} else {
					// Secondary mode: network synchronization
					contexts[i].server_ip = interface_or_ip;
					contexts[i].mac_address = mac_address;
					contexts[i].primary_pipe = -1;
				}

				contexts[i].pipe = pipes_vec[i];
				contexts[i].delta = delta;
				contexts[i].timestamps = timestamps;
				contexts[i].shift = shift;
				contexts[i].time_period = time_period;
				contexts[i].learning_rate = learning_rate;
				contexts[i].shift2 = shift2;
				contexts[i].overshoot_ratio = overshoot_ratio;
				contexts[i].step_threshold = step_threshold;
				contexts[i].wait_between_steps = wait_between_steps;

				int create_ret = pthread_create(&threads[i], nullptr, pipe_sync_thread, &contexts[i]);
				if (create_ret != 0) {
					ERR("Failed to create thread for pipe %d: error %d (%s)\n",
						pipes_vec[i], create_ret, strerror(create_ret));
				} else {
					thread_created[i] = true;
					INFO("Started %s sync thread for pipe %d (thread_id: %lu)\n",
						is_pipelock ? "pipelock" : "secondary", pipes_vec[i], (unsigned long)threads[i]);
				}
			} catch (const std::exception& e) {
				ERR("Exception during thread creation for pipe %d: %s\n", pipes_vec[i], e.what());
			} catch (...) {
				ERR("Unknown exception during thread creation for pipe %d\n", pipes_vec[i]);
			}
		}

		// Wait for all threads to complete
		for (size_t i = 0; i < threads.size(); ++i) {
			try {
				if (!thread_created[i]) {
					INFO("Skipping join for pipe %d (thread was not created)\n", pipes_vec[i]);
					continue;
				}

				INFO("Joining thread for pipe %d (thread_id: %lu)...\n",
					pipes_vec[i], (unsigned long)threads[i]);

				int join_ret = pthread_join(threads[i], NULL);
				if (join_ret != 0) {
					ERR("Failed to join thread for pipe %d: error %d (%s)\n",
						pipes_vec[i], join_ret, strerror(join_ret));
				} else {
					INFO("%s sync thread for pipe %d completed successfully\n",
						is_pipelock ? "Pipelock" : "Secondary", pipes_vec[i]);
				}
			} catch (const std::exception& e) {
				ERR("Exception during thread join for pipe %d: %s\n", pipes_vec[i], e.what());
			} catch (...) {
				ERR("Unknown exception during thread join for pipe %d\n", pipes_vec[i]);
			}
		}

		// For pipelock mode, stop the primary collector
		if (is_pipelock) {
			stop_vsync_collector();
		}
	} else {
		ERR("Invalid mode: %s. Use 'pri', 'sec', or 'pipelock'\n", modeStr.c_str());
		return 1;
	}

	vsync_lib_uninit();

	if(server) {
		delete server;
		server = nullptr;
	}
	return ret;
}
