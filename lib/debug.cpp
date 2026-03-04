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

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <mutex>
#include "debug.h"
#include "system_platform.h"

log_level dbg_lvl = LOG_LEVEL_INFO;
std::string mode_str = "";
bool time_init = false;
os_timespec base_time;

// Thread-local pipe context (-1 means no pipe context set)
static thread_local int current_pipe_id = -1;

// Mutex for thread-safe logging - C++11 standard mutex
static std::mutex log_mutex;

/**
* @brief
* This function sets the logging level for the application.
* The logging level determines the severity of messages that will be logged.
*
* @param level - The logging level to set, specified as a LogLevel enum value.
* @return 0 - success, non zero - failure
*/
int set_log_level(log_level level)
{
	if (level < LOG_LEVEL_NONE || level > LOG_LEVEL_TRACE) {
		ERR("Invalid log level: %d\n", level);
		return 1;
	}

	dbg_lvl = level;
	return 0;
}

/**
* @brief
* This function sets the logging level for the application via string parameter.
* The logging level determines the severity of messages that will be logged.
*
* @param level - The logging level to set, specified as a string value.
* @return 0 - success, non zero - failure
*/
extern "C" int set_log_level_str(const char* log_level)
{
	int status = 1;

	if (log_level == nullptr) {
		ERR("NULL log level provided\n");
		return 1;
	}

	if (strcasecmp(log_level, "error") == 0) {
		status = set_log_level(LOG_LEVEL_ERROR);
	} else if (strcasecmp(log_level, "warning") == 0) {
		status = set_log_level(LOG_LEVEL_WARNING);
	} else if (strcasecmp(log_level, "info") == 0) {
		status = set_log_level(LOG_LEVEL_INFO);
	} else if (strcasecmp(log_level, "debug") == 0) {
		status = set_log_level(LOG_LEVEL_DEBUG);
	} else if (strcasecmp(log_level, "trace") == 0) {
		status = set_log_level(LOG_LEVEL_TRACE);
	}

	return status;
}

/**
* @brief
* This function gets the current logging level for the application.
*
* @return The current log_level enum value
*/
extern "C" log_level get_log_level(void)
{
	return dbg_lvl;
}

/**
* @brief
* This function gets the current logging level for the application as a string.
*
* @return A string representation of the current log level
*/
extern "C" const char* get_log_level_str(void)
{
	switch (dbg_lvl) {
		case LOG_LEVEL_NONE:    return "none";
		case LOG_LEVEL_ERROR:   return "error";
		case LOG_LEVEL_WARNING: return "warning";
		case LOG_LEVEL_INFO:    return "info";
		case LOG_LEVEL_DEBUG:   return "debug";
		case LOG_LEVEL_TRACE:   return "trace";
		default:                return "unknown";
	}
}

/**
* @brief
* This function sets the logging mode for the application.
* The logging mode determines the source for the log messages.
* (PRIMARY, SECONDARY, VBLTEST, SYNCTEST).
*
* @param mode - A string representing the run mode (e.g., "PRIMARY", "SECONDARY").
* @return 0 - success, non zero - failure
*/
extern "C" int set_log_mode(const char* mode)
{

	if (mode == nullptr) {
		ERR("NULL log mode provided\n");
		return 1;
	}

	mode_str = mode;

	return 0;
}

/**
* @brief
* Set the pipe ID for the current thread.
* This allows log messages from this thread to automatically include the pipe identifier.
*
* @param pipe_id - The pipe ID (0-3), or -1 to clear
* @return void
*/
void set_thread_pipe_id(int pipe_id)
{
	current_pipe_id = pipe_id;
}

/**
* @brief
* Get the current thread's pipe ID.
*
* @return The pipe ID, or -1 if not set
*/
int get_thread_pipe_id(void)
{
	return current_pipe_id;
}

/**
* @brief
* Clear the pipe ID for the current thread.
*
* @return void
*/
void clear_thread_pipe_id(void)
{
	current_pipe_id = -1;
}

/**
* @brief
* This function logs a message at the specified logging level.
* It supports formatted output similar to printf-style formatting.
* Automatically includes pipe ID if set via set_thread_pipe_id().
* Thread-safe: uses mutex to ensure atomic log output.
*
* @param level - The logging level of the message, specified as a LogLevel enum value.
* @param format - A format string that specifies how to format the message.
* @param ... - Additional arguments to be formatted according to the format string.
* @return void
*/
void log_message(log_level level, const char* format, ...)
{
	os_timespec now, diff;

	// Fast path: check log level before taking the lock
	if (level > dbg_lvl) {
		return;
	}

	// RAII lock - automatically unlocks when leaving scope
	std::lock_guard<std::mutex> lock(log_mutex);

	// If this is the first time then initialize the base time
	if (time_init == false) {
		os_clock_gettime(OS_CLOCK_MONOTONIC, &base_time);
		time_init = true;
	}

	os_clock_gettime(OS_CLOCK_MONOTONIC, &now);

	// Calculate the time difference from base_time
	if ((now.tv_nsec - base_time.tv_nsec) < 0) {
		diff.tv_sec = now.tv_sec - base_time.tv_sec - 1;
		diff.tv_nsec = now.tv_nsec - base_time.tv_nsec + 1000000000;
	} else {
		diff.tv_sec = now.tv_sec - base_time.tv_sec;
		diff.tv_nsec = now.tv_nsec - base_time.tv_nsec;
	}

	// Convert nanoseconds to milliseconds
	int msec = diff.tv_nsec / 1000000;

	const char* level_str = nullptr;
	switch (level) {
		case LOG_LEVEL_NONE:
			level_str = "[PRNT]";
			break;
		case LOG_LEVEL_ERROR:
			level_str = "[ERR ]";
			break;
		case LOG_LEVEL_WARNING:
			level_str = "[WARN]";
			break;
		case LOG_LEVEL_INFO:
			level_str = "[INFO]";
			break;
		case LOG_LEVEL_DEBUG:
			level_str = "[DBG ]";
			break;
		case LOG_LEVEL_TRACE:
			level_str = "[TRACE]";
			break;
		default:
			level_str = "";
			break;
	}

	// Include pipe ID if set for this thread
	if (current_pipe_id >= 0) {
		printf("%s%s[P%d][%4ld.%03d] ", mode_str.c_str(), level_str,
				current_pipe_id, diff.tv_sec, msec);
	} else {
		printf("%s%s[Px][%4ld.%03d] ", mode_str.c_str(), level_str, diff.tv_sec, msec);
	}

	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	fflush(stdout);

	// lock_guard automatically unlocks when it goes out of scope
}
