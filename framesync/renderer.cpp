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

#include "renderer.h"
#include <SDL2/SDL.h>

Renderer::Renderer(const DemoConfig& config)
	: 	m_config(config),
		m_window(nullptr),
		m_renderer(nullptr),
		m_vsync_lib_initialized(false),
		m_current_color(0x000000FF),
		m_frame_count(0),
		m_initialized(false),
		m_is_fullscreen(config.fullscreen)
{
}

Renderer::~Renderer() {
	cleanup();
}

int Renderer::init() {
	if (m_initialized) {
		WARNING("Renderer already initialized\n");
		return 0;
	}

	// Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		ERR("SDL initialization failed: %s\n", SDL_GetError());
		return -1;
	}

	// Create window with dynamic title showing mode and configuration
	std::string window_title = "FrameSync - ";
	window_title += (m_config.mode == "primary") ? "Primary" : "Secondary";
	window_title += " [Pipe " + std::to_string(m_config.pipe) + "]";
	if (m_config.mode == "secondary") {
		window_title += " @ " + m_config.multicast_group;
	}

	uint32_t flags = SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE;
	if (m_config.fullscreen) {
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}

	m_window = SDL_CreateWindow(
		window_title.c_str(),
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		m_config.width,
		m_config.height,
		flags
	);

	if (!m_window) {
		ERR("Window creation failed: %s\n", SDL_GetError());
		SDL_Quit();
		return -1;
	}

	// Create renderer with vsync enabled
	uint32_t renderer_flags = SDL_RENDERER_ACCELERATED;
	if (m_config.vsync_enabled) {
		renderer_flags |= SDL_RENDERER_PRESENTVSYNC;
	}

	m_renderer = SDL_CreateRenderer(m_window, -1, renderer_flags);
	if (!m_renderer) {
		ERR("Renderer creation failed: %s\n", SDL_GetError());
		SDL_DestroyWindow(m_window);
		SDL_Quit();
		return -1;
	}

	// Initialize vsyncalter library for vblank access
	if (vsync_lib_init(m_config.device.c_str(), false, false) != 0) {
		ERR("Failed to initialize vsyncalter library\n");
		SDL_DestroyRenderer(m_renderer);
		SDL_DestroyWindow(m_window);
		SDL_Quit();
		return -1;
	}
	m_vsync_lib_initialized = true;

	m_initialized = true;
	INFO("Renderer initialized: %dx%d, pipe=%d, vsync=%s\n",
		m_config.width, m_config.height, m_config.pipe,
		m_config.vsync_enabled ? "ON" : "OFF");

	return 0;
}

void Renderer::cleanup() {
	if (!m_initialized) {
		return;
	}

	if (m_vsync_lib_initialized) {
		vsync_lib_uninit();
		m_vsync_lib_initialized = false;
	}

	if (m_renderer) {
		SDL_DestroyRenderer(m_renderer);
		m_renderer = nullptr;
	}

	if (m_window) {
		SDL_DestroyWindow(m_window);
		m_window = nullptr;
	}

	SDL_Quit();
	m_initialized = false;
	INFO("Renderer cleaned up\n");
}

void Renderer::render_solid_color(uint32_t color) {
	if (!m_initialized) {
		return;
	}

	uint8_t r, g, b, a;
	color_to_rgba(color, r, g, b, a);

	// Set draw color and clear screen
	SDL_SetRenderDrawColor(m_renderer, r, g, b, a);
	SDL_RenderClear(m_renderer);

	m_current_color = color;
}

void Renderer::present() {
	if (!m_initialized) {
		return;
	}

	SDL_RenderPresent(m_renderer);
	m_frame_count++;
}

int Renderer::wait_for_vblank(uint64_t& timestamp_us) {
	if (!m_initialized || !m_vsync_lib_initialized) {
		return -1;
	}

	// Use get_vsync with size=1 to wait for next vblank
	uint64_t vblank_timestamp = 0;
	if (get_vsync(m_config.device.c_str(), &vblank_timestamp, 1, m_config.pipe) != 0) {
		ERR("get_vsync failed\n");
		return -1;
	}

	timestamp_us = vblank_timestamp;
	return 0;
}

bool Renderer::should_close() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT) {
			return true;
		}
		if (event.type == SDL_KEYDOWN) {
			if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_q) {
				return true;
			}
			// F11 toggles fullscreen
			if (event.key.keysym.sym == SDLK_F11) {
				toggle_fullscreen();
			}
		}
		// Handle window resize events
		if (event.type == SDL_WINDOWEVENT) {
			if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
				DBG("Window resized to %dx%d\n", event.window.data1, event.window.data2);
			}
		}
	}
	return false;
}

void Renderer::toggle_fullscreen() {
	if (!m_initialized || !m_window) {
		return;
	}

	m_is_fullscreen = !m_is_fullscreen;

	if (m_is_fullscreen) {
		if (SDL_SetWindowFullscreen(m_window, SDL_WINDOW_FULLSCREEN_DESKTOP) != 0) {
			WARNING("Failed to enter fullscreen: %s\n", SDL_GetError());
			m_is_fullscreen = false;
		} else {
			INFO("Entered fullscreen mode\n");
		}
	} else {
		if (SDL_SetWindowFullscreen(m_window, 0) != 0) {
			WARNING("Failed to exit fullscreen: %s\n", SDL_GetError());
			m_is_fullscreen = true;
		} else {
			INFO("Exited fullscreen mode\n");
		}
	}
}
