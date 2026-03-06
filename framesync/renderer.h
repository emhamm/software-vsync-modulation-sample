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

#ifndef _RENDERER_H
#define _RENDERER_H

#include "demo_common.h"
#include <SDL2/SDL.h>

/**
 * @brief Renderer class for synchronized display output
 *
 * Manages SDL window/renderer and coordinates rendering with vblank timing
 */
class Renderer {
public:
	Renderer(const DemoConfig& config);
	~Renderer();

	/**
	 * Initialize SDL and create window/renderer
	 * @return 0 on success, negative on error
	 */
	int init();

	/**
	 * Clean up SDL resources
	 */
	void cleanup();

	/**
	 * Fill screen with solid color
	 * @param color RGBA8888 color value
	 */
	void render_solid_color(uint32_t color);

	/**
	 * Present the rendered frame (swap buffers)
	 */
	void present();

	/**
	 * Wait for next vblank and get timestamp
	 * @param timestamp_us Output: vblank timestamp in microseconds
	 * @return 0 on success, negative on error
	 */
	int wait_for_vblank(uint64_t& timestamp_us);

	/**
	 * Check if window should close (user pressed X or ESC)
	 */
	bool should_close();

	/**
	 * Toggle fullscreen mode
	 */
	void toggle_fullscreen();

private:
	const DemoConfig& m_config;
	SDL_Window* m_window;
	SDL_Renderer* m_renderer;
	bool m_vsync_lib_initialized;
	uint32_t m_current_color;
	uint64_t m_frame_count;
	bool m_initialized;
	bool m_is_fullscreen;
};

#endif // _RENDERER_H
