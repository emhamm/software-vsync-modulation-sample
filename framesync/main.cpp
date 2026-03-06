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
#include <getopt.h>
#include <csignal>

// Global shutdown flag
std::atomic<bool> g_shutdown(false);

void signal_handler(int sig) {
	INFO("\nReceived signal %d, shutting down...\n", sig);
	g_shutdown = true;
}

void setup_signal_handlers() {
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
}

static void print_usage(const char* prog_name) {
	printf("FrameSync - Synchronized Frame Presentation Demo\n");
	printf("Usage: %s [options]\n\n", prog_name);
	printf("Options:\n");
	printf("  -m, --mode <mode>         Operating mode: primary or secondary (default: primary)\n");
	printf("  -g, --multicast-group <ip> UDP multicast group address (default: 239.1.1.1)\n");
	printf("  -p, --multicast-port <port> UDP multicast port (default: 5001)\n");
	printf("  -T, --multicast-ttl <ttl>  Multicast TTL: 1=subnet, 32=campus (default: 1)\n");
	printf("  -I, --multicast-if <ip>    Multicast interface IP (default: auto-select)\n");
	printf("  -P, --pipe <pipe>         Display pipe/connector to use (default: 0)\n");
	printf("  -d, --device <device>     DRM device path (default: /dev/dri/card0)\n");
	printf("  -t, --interval <ms>       Color change interval in milliseconds (default: 1000)\n");
	printf("  -s, --schedule-ahead <ms> Schedule color changes this many ms ahead (default: 33)\n");
	printf("  -f, --fullscreen          Run in fullscreen mode\n");
	printf("  -W, --width <width>       Window width (default: 1920)\n");
	printf("  -H, --height <height>     Window height (default: 1080)\n");
	printf("  -v, --log-level <level>   Log level: error, warning, info, debug or trace (default: warning)\n");
	printf("  -n, --no-vsync            Disable vsync (not recommended)\n");
	printf("  -c, --no-cycle            Disable automatic color cycling\n");
	printf("  -h, --help                Display this help message\n");
	printf("\nExample Usage:\n");
	printf("  Primary (local):   %s -m primary -g 239.1.1.1 -p 5001 -P 0\n", prog_name);
	printf("  Secondary (local): %s -m secondary -g 239.1.1.1 -p 5001 -P 1\n", prog_name);
	printf("  Primary (network): %s -m primary -g 239.1.1.1 -I 192.168.0.10\n", prog_name);
	printf("  Secondary (network): %s -m secondary -g 239.1.1.1 -I 192.168.0.11\n", prog_name);
	printf("\nNotes:\n");
	printf("  - Uses UDP multicast for simultaneous color updates across all displays\n");
	printf("  - Supports mixed deployments: local + network clients simultaneously\n");
	printf("  - Multicast loopback is always enabled to support all deployment scenarios\n");
	printf("  - No TCP connection needed - secondaries just join the multicast group\n");
	printf("  - Ensure swgenlock is running for vblank synchronization between systems\n");
	printf("  - Primary coordinates when color changes occur across all displays\n");
	printf("  - All systems change color at the same real-time timestamp\n");
	printf("  - Systems must have synchronized clocks (PTP or Chrony)\n");
	printf("  - Schedule-ahead should be > network latency (typically 30-100ms)\n");
	printf("  - Multicast group 239.x.x.x is for private LANs (like 192.168.x.x)\n");
	printf("  - Press ESC or Q to quit, or use Ctrl+C\n");
	printf("  - Press F11 to toggle fullscreen mode\n");
	printf("  - Window is resizable (drag edges/corners)\n");
}

int main(int argc, char* argv[]) {
	// Set default log level to warning
	set_log_level_str("warning");

	DemoConfig config;

	static struct option long_options[] = {
		{"mode",        required_argument, 0, 'm'},
		{"multicast-group", required_argument, 0, 'g'},
		{"multicast-port", required_argument, 0, 'p'},
		{"multicast-ttl", required_argument, 0, 'T'},
		{"multicast-if", required_argument, 0, 'I'},
		{"pipe",        required_argument, 0, 'P'},
		{"device",      required_argument, 0, 'd'},
		{"schedule-ahead", required_argument, 0, 's'},
		{"interval",    required_argument, 0, 't'},
		{"fullscreen",  no_argument,       0, 'f'},
		{"width",       required_argument, 0, 'W'},
		{"height",      required_argument, 0, 'H'},
		{"log-level",   required_argument, 0, 'v'},
		{"no-vsync",    no_argument,       0, 'n'},
		{"no-cycle",    no_argument,       0, 'c'},
		{"help",        no_argument,       0, 'h'},
		{0, 0, 0, 0}
	};

	int opt;
	int option_index = 0;

	while ((opt = getopt_long(argc, argv, "m:g:p:T:I:P:d:t:s:fW:H:v:nch", long_options, &option_index)) != -1) {
		switch (opt) {
			case 'm':
				config.mode = optarg;
				if (config.mode != "primary" && config.mode != "secondary") {
					ERR("Invalid mode: %s (must be 'primary' or 'secondary')\n", optarg);
					return 1;
				}
				break;
			case 'g':
				config.multicast_group = optarg;
				break;
			case 'p':
				config.multicast_port = atoi(optarg);
				if (config.multicast_port <= 0 || config.multicast_port > 65535) {
					ERR("Invalid multicast port: %s\n", optarg);
					return 1;
				}
				break;
			case 'T':
				config.multicast_ttl = atoi(optarg);
				if (config.multicast_ttl < 1 || config.multicast_ttl > 255) {
					ERR("Invalid multicast TTL: %s (must be 1-255)\n", optarg);
					return 1;
				}
				break;
			case 'I':
				config.multicast_if = optarg;
				break;
			case 'P':
				config.pipe = atoi(optarg);
				if (config.pipe < 0) {
					ERR("Invalid pipe: %s\n", optarg);
					return 1;
				}
				break;
			case 'd':
				config.device = optarg;
				break;
			case 't':
				config.interval_ms = atoi(optarg);
				if (config.interval_ms < 0) {
					ERR("Invalid interval: %s\n", optarg);
					return 1;
				}
				break;
			case 's':
				config.schedule_ahead_ms = atoi(optarg);
				if (config.schedule_ahead_ms < 10) {
					ERR("Invalid schedule-ahead (must be >= 10ms): %s\n", optarg);
					return 1;
				}
				break;
			case 'f':
				config.fullscreen = true;
				break;
			case 'W':
				config.width = atoi(optarg);
				break;
			case 'H':
				config.height = atoi(optarg);
				break;
			case 'v':
				set_log_level_str(optarg);
				break;
			case 'n':
				config.vsync_enabled = false;
				WARNING("VSync disabled - synchronization may not work correctly\n");
				break;
			case 'c':
				config.cycle_colors = false;
				break;
			case 'h':
				print_usage(argv[0]);
				return 0;
			default:
				print_usage(argv[0]);
				return 1;
		}
	}

	// Setup signal handlers
	setup_signal_handlers();

	// Print configuration
	INFO("VSync Demo starting...\n");
	INFO("Configuration:\n");
	INFO("  Mode: %s\n", config.mode.c_str());
	INFO("  Multicast: %s:%d (TTL=%d)\n", config.multicast_group.c_str(),
	     config.multicast_port, config.multicast_ttl);
	INFO("  Device: %s\n", config.device.c_str());
	INFO("  Pipe: %d\n", config.pipe);
	if (config.mode == "primary") {
		INFO("  Interval: %d ms\n", config.interval_ms);
		INFO("  Schedule ahead: %d ms\n", config.schedule_ahead_ms);
	}
	INFO("  Display: %s%s\n",
		config.fullscreen ? "Fullscreen" : "Windowed",
		config.fullscreen ? "" : (" (" + std::to_string(config.width) + "x" + std::to_string(config.height) + ")").c_str());
	INFO("  VSync: %s\n", config.vsync_enabled ? "Enabled" : "Disabled");
	INFO("  Auto-cycle colors: %s\n", config.cycle_colors ? "Yes" : "No");
	INFO("\n");

	// Run in selected mode
	int ret;
	if (config.mode == "primary") {
		ret = run_primary(config);
	} else {
		ret = run_secondary(config);
	}

	if (ret == 0) {
		INFO("VSync Demo completed successfully\n");
	} else {
		ERR("VSync Demo failed with error code %d\n", ret);
	}

	return ret;
}
