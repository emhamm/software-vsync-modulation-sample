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

#ifndef _NETWORK_PLATFORM_H
#define _NETWORK_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

// Cross-platform socket type
#ifdef __linux__
typedef int os_socket_t;
#define OS_INVALID_SOCKET -1
#endif

/**
 * @brief Close a socket (works for both client and server sockets)
 * @param sockfd Socket file descriptor
 * @return 0 on success, -1 on error
 */
int os_close_socket(os_socket_t sockfd);

/**
 * @brief Initialize network subsystem (required on Windows, no-op on Linux)
 * @return 0 on success, -1 on error
 */
int os_network_init(void);

/**
 * @brief Cleanup network subsystem (required on Windows, no-op on Linux)
 * @return void
 */
void os_network_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // _NETWORK_PLATFORM_H
