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

#include "system_platform.h"
#include "debug.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <errno.h>

device_handle_t os_open_device(const char* device_str) {
	if (!device_str) {
		return INVALID_DEVICE_HANDLE;
	}
	return open(device_str, O_RDWR | O_CLOEXEC, 0);
}

void os_close_device(device_handle_t handle) {
	if (handle != INVALID_DEVICE_HANDLE && close(handle) == -1) {
		ERR("Failed to properly close file descriptor. Error: %s\n", strerror(errno));
	}
}

int os_make_timer(long expire_ms, void* user_ptr, reset_func callback, void** timer_id) {
	struct sigevent te;
	struct itimerspec its;
	struct sigaction sa;
	int sig_no = SIGRTMIN;
	timer_t* tid = (timer_t*)timer_id;

	INFO("Setting timer for %.3f seconds\n", expire_ms/1000.0);

	// Set up signal handler
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = callback;
	sigemptyset(&sa.sa_mask);
	if (sigaction(sig_no, &sa, nullptr) == -1) {
		ERR("Failed to setup signal handling for timer.\n");
		return 1;
	}

	// Create timer
	te = {};
	te.sigev_notify = SIGEV_SIGNAL;
	te.sigev_signo = sig_no;
	te.sigev_value.sival_ptr = user_ptr;
	if (timer_create(CLOCK_REALTIME, &te, tid) != 0) {
		ERR("Failed to create timer: %s\n", strerror(errno));
		return 1;
	}

	// Set timer interval and value
	its.it_interval.tv_sec = 0;
	its.it_interval.tv_nsec = (expire_ms % 1000) * 1000000L;
	its.it_value.tv_sec = expire_ms / 1000;
	its.it_value.tv_nsec = (expire_ms % 1000) * 1000000L;
	if (timer_settime(*tid, 0, &its, nullptr) != 0) {
		ERR("Failed to set timer: %s\n", strerror(errno));
		return 1;
	}

	return 0;
}

int os_stat_file(const char* path) {
	if (!path) {
		return -1;
	}
	struct stat st;
	return stat(path, &st);
}

int os_clock_gettime(os_clock_type clock_id, os_timespec* tp) {
	if (!tp) {
		return -1;
	}

	struct timespec ts;
	clockid_t posix_clock;

	// Map platform clock type to POSIX clock type
	switch (clock_id) {
		case OS_CLOCK_REALTIME:
			posix_clock = CLOCK_REALTIME;
			break;
		case OS_CLOCK_MONOTONIC:
			posix_clock = CLOCK_MONOTONIC;
			break;
		default:
			return -1;
	}

	int result = clock_gettime(posix_clock, &ts);
	if (result == 0) {
		tp->tv_sec = ts.tv_sec;
		tp->tv_nsec = ts.tv_nsec;
	}

	return result;
}
