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

#ifndef _DEMO_PROTOCOL_H
#define _DEMO_PROTOCOL_H

#include <cstdint>

// Protocol version for compatibility checking
#define DEMO_PROTOCOL_VERSION 1

// Message types
enum DemoMessageType {
	MSG_HANDSHAKE = 1,
	MSG_COLOR_UPDATE = 2,
	MSG_ACK = 3,
	MSG_STATUS = 4
};

// Handshake message - sent by secondary to primary
struct DemoHandshakeMsg {
	uint32_t protocol_version;
	uint32_t pipe_id;
	char hostname[64];
} __attribute__((packed));

// Color update command - sent by primary to all secondaries
struct DemoColorUpdateMsg {
	uint64_t target_timestamp_us;  // Real-time timestamp to update at (microseconds, CLOCK_REALTIME)
	uint32_t color;                // RGBA8888 color
	uint32_t pattern_type;         // 0=solid, 1=gradient, 2=checkerboard, etc.
	uint64_t sequence_num;         // Sequence number for tracking
} __attribute__((packed));

// Acknowledgment message
struct DemoAckMsg {
	uint64_t sequence_num;
	uint32_t status;  // 0=success, non-zero=error code
} __attribute__((packed));

// Status message for debugging
struct DemoStatusMsg {
	uint64_t current_vblank;
	uint64_t frames_rendered;
	double fps;
} __attribute__((packed));

// Generic message container
struct DemoMessage {
	uint8_t type;
	uint8_t reserved[3];
	uint32_t payload_size;
	union {
		DemoHandshakeMsg handshake;
		DemoColorUpdateMsg color_update;
		DemoAckMsg ack;
		DemoStatusMsg status;
		uint8_t raw[256];
	} payload;
} __attribute__((packed));

// Default port for demo communication
#define DEMO_DEFAULT_PORT 5555

// Predefined colors for cycling
static const uint32_t DEMO_COLORS[] = {
	0xFF0000FF,  // Red
	0x00FF00FF,  // Green
	0x0000FFFF,  // Blue
	0xFFFF00FF,  // Yellow
	0xFF00FFFF,  // Magenta
	0x00FFFFFF,  // Cyan
	0xFFFFFFFF,  // White
	0x000000FF   // Black
};

#define DEMO_NUM_COLORS (sizeof(DEMO_COLORS) / sizeof(DEMO_COLORS[0]))

#endif // _DEMO_PROTOCOL_H
