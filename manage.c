// manage.c -- Core window layout logic for JrWM

// This file is responsible for the layout and rendering of windows, making it
// responsible for much of the core "look and feel" of the WM.

// JrWM is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with
// this program. If not, see <https://www.gnu.org/licenses/>.
//
// Copyright 2026 Isaac Freund, 2026 Jack Conger.  All rights reserved.

#include "jrwm.h"


// Settings for layout and borders

#define	COLOR(hex)	{ ((hex >> 24) & 0xFF) * (UINT32_MAX / 255), \
			  ((hex >> 16) & 0xFF) * (UINT32_MAX / 255), \
			  ((hex >>  8) & 0xFF) * (UINT32_MAX / 255), \
			  ( hex        & 0xFF) * (UINT32_MAX / 255) }

static uint32_t bordercolor[4] = COLOR(0x333333ff);
static uint32_t focusedcolor[4] = COLOR(0x77aa99ff);
static int borderpx = 2;
static float splitratio = 0.52;


// The main WM loops, which lay out windows and render borders.

static void layout_space(struct Space *space, struct Rect bounds) {
	int count = 0, w = 0, rightwidth = bounds.width, rightheight = bounds.height;
	struct Window *window;
	wl_list_for_each(window, &wm.windows, link) {
		if (window->space == space)
			count++;
	}
	wl_list_for_each(window, &wm.windows, link) {
		if (window->space != space)
			continue;
		struct Rect wlay;
		if (count == 1) {
			// Only window
			wlay.x = bounds.x + borderpx;
			wlay.y = bounds.y + borderpx;
			wlay.width = bounds.width - borderpx * 2;
			wlay.height = bounds.height - borderpx * 2;
		} else if (w == 0) {
			// Left side "main" window
			wlay.x = bounds.x + borderpx;
			wlay.y = bounds.y + borderpx;
			wlay.width = bounds.width * splitratio - borderpx * 2;
			wlay.height = bounds.height - borderpx * 2;

			rightwidth -= wlay.width + borderpx;
		} else {
			// Right side "stacked" windows
			wlay.x = bounds.x + bounds.width - rightwidth + borderpx;
			wlay.y = bounds.y + bounds.height - rightheight + borderpx;
			wlay.width = rightwidth - borderpx * 2;
			wlay.height = rightheight / (count - w) - borderpx * 2;

			rightheight -= wlay.height + borderpx;
		}
		window->layout.x = wlay.x;
		window->layout.y = wlay.y;
		window->layout.width = wlay.width;
		window->layout.height = wlay.height;
		w++;
	}
}

static bool valid_rect(struct Rect r) {
	return r.width >= 0 && r.height >= 0;
}

extern void seat_do_focus(struct Seat *seat) {
	if (seat->focused->focused != NULL)
		river_seat_v1_focus_window(seat->obj, seat->focused->focused->obj);
	else
		river_seat_v1_clear_focus(seat->obj);
}

extern void window_do_deferred(struct Window *window) {
	if (window->set_capabilities) {
		river_window_v1_set_capabilities(window->obj,
				RIVER_WINDOW_V1_CAPABILITIES_MAXIMIZE |
				RIVER_WINDOW_V1_CAPABILITIES_FULLSCREEN);
		window->set_capabilities = false;
	}
	if (window->close) {
		river_window_v1_close(window->obj);
		window->close = false;
	}
	if (window->fullscreen) {
		river_window_v1_inform_fullscreen(window->obj);
		window->fullscreen = false;
	}
	if (window->exit_fullscreen) {
		river_window_v1_exit_fullscreen(window->obj);
		river_window_v1_inform_not_fullscreen(window->obj);
		window->exit_fullscreen = false;
	}
	if (window->maximize) {
		river_window_v1_inform_maximized(window->obj);
		window->maximize = false;
	}
	if (window->unmaximize) {
		river_window_v1_inform_unmaximized(window->obj);
		window->unmaximize = false;
	}
}

extern void manage_output(struct Output *output) {
	struct Window *window;
	struct Space *space = output->active;
	if (space->fullscreen != NULL) {
		river_window_v1_fullscreen(space->fullscreen->obj, output->obj);
	} else if (space->maximized != NULL) {
		window = space->maximized;
		window->layout.x = output->windowed.x;
		window->layout.y = output->windowed.y;
		window->layout.width = output->windowed.width;
		window->layout.height = output->windowed.height;
		river_window_v1_propose_dimensions(window->obj,
				window->layout.width,
				window->layout.height);
	} else {
		layout_space(space, output->windowed);
		wl_list_for_each(window, &wm.windows, link) {
			if (window->space == output->active) {
				river_window_v1_use_ssd(window->obj);
				river_window_v1_set_tiled(window->obj, 15);
				if (valid_rect(window->layout))
					river_window_v1_propose_dimensions(window->obj,
							window->layout.width,
							window->layout.height);
			}
		}
	}
}

extern void render_output(struct Output *output) {
	struct Space *space = output->active;
	struct Window *window;
	wl_list_for_each(window, &wm.windows, link) {
		if (window->space != space) {
			river_window_v1_hide(window->obj);
			continue;
		}
		if (space->fullscreen != NULL) {
			// only show fullscreen window
			if (space->fullscreen == window)
				river_window_v1_show(window->obj);
			else
				river_window_v1_hide(window->obj);
			continue;
		}
		if (space->maximized != NULL) {
			// only show maximized window and any children
			if (space->maximized == window) {
				river_window_v1_show(window->obj);
				river_node_v1_place_top(window->node);
				river_window_v1_set_borders(window->obj, 15, 0,
						0, 0, 0, 0);
				river_node_v1_set_position(window->node,
						window->layout.x,
						window->layout.y);
			} else {
				river_window_v1_hide(window->obj);
			}
			continue;
		}
		river_window_v1_show(window->obj);
		if (window == window->space->focused) {
			river_node_v1_place_top(window->node);
			river_window_v1_set_borders(window->obj, 15, 2,
					focusedcolor[0],
					focusedcolor[1],
					focusedcolor[2],
					focusedcolor[3]);
		} else {
			river_window_v1_set_borders(window->obj, 15, 2,
					bordercolor[0],
					bordercolor[1],
					bordercolor[2],
					bordercolor[3]);
		}
		river_node_v1_set_position(window->node,
				window->layout.x,
				window->layout.y);
	}
}
