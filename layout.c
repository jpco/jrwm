// layout.c -- Core window layout+rendering logic for JrWM

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

static uint32_t border_color[4] = COLOR(0x333333ff);
static uint32_t focused_color[4] = COLOR(0x77aa99ff);
static uint32_t semi_focused_color[4] = COLOR(0x555555ff);

static int monocle_borderpx = 0;
static int tiled_borderpx = 2;
static float tiled_splitratio = 0.52;


// Private functions for window management and rendering

// Return the Output on which this Space is active, or NULL if it is not active
// on any Output.
static struct Output *active_on_output(struct Space *space) {
	struct Output *output = NULL;
	if (space->output != NULL && space->output->active == space)
		output = space->output;
	return output;
}

static bool valid_rect(struct Rect r) {
	return r.width >= 0 && r.height >= 0;
}


// Handle creation/deletion of the core objects that all point to each other.
//  - These run before the relevant wl_list in wm is updated
//  - None of these run during a manage or render sequence

// Find a Space for this Output to activate
// FIXME: What if all Spaces already have an Output?
extern void place_output(struct Output *output) {
	struct Space *space;

	// Send any Spaces with no Output here
	wl_list_for_each(space, &wm.spaces, link)
		if (space->output == NULL)
			space->output = output;

	// If we have a focused space, use that as our active one
	struct Seat *seat;
	wl_list_for_each(seat, &wm.seats, link)
		if (output == seat->focused->output)
			output->active = seat->focused;

	// Otherwise, just pick the first Space with this Output
	if (output->active == NULL) {
		wl_list_for_each(space, &wm.spaces, link) {
			if (output == space->output) {
				output->active = space;
				break;
			}
		}
	}
}

// Replace this Output with another for any relevant Spaces
extern void replace_output(struct Output *output) {
	struct Output *replacement = NULL, *r;
	struct Space *space;

	// Pick a random other Output, if there are any.
	wl_list_for_each(r, &wm.outputs, link)
		if (r != output)
			replacement = r;

	// Assign that Output (or NULL, if no other Outputs exist) to any Spaces
	wl_list_for_each(space, &wm.spaces, link)
		if (space->output == output)
			space->output = replacement;
}

// Find a Space for this Window to be in
extern void place_window(struct Window *window) {
	struct Seat *seat;
	struct Space *space;

	// Use a random Seat's focused Space
	// TODO: How to use a better Seat here?
	wl_list_for_each(seat, &wm.seats, link) {
		space = seat->focused;
		window->space = space;
		space->focused = window;
	}

	// Fallback: pick a random Space
	if (window->space == NULL)
		wl_list_for_each(space, &wm.spaces, link)
			window->space = space;

	// If the Space has no focused Window, focus this one
	if (window->space->focused == NULL)
		window->space->focused = window;
}

// Replace this Window with another for any relevant Spaces
extern void replace_window(struct Window *window) {
	struct Space *space;
	wl_list_for_each(space, &wm.spaces, link) {
		if (space->focused != window)
			continue;
		struct Window *r, *replacement = NULL;
		// Find a random other Window to focus in the Space
		// (or NULL, if no other Window exists)
		wl_list_for_each(r, &wm.windows, link) {
			if (r->space == space && r != window)
				replacement = r;
		}
		space->focused = replacement;
	}
}

// Find a Space for this Seat to focus on
extern void place_seat(struct Seat *seat) {
	struct Output *output;
	struct Space *space;

	// Pick a random Output's active Space
	wl_list_for_each(output, &wm.outputs, link)
		seat->focused = output->active;

	// Fallback: Pick a random Space
	if (seat->focused == NULL)
		wl_list_for_each(space, &wm.spaces, link)
			seat->focused = space;
}


// Space layout functions

extern void monocle_layout(struct Space *space, struct Rect bounds) {
	struct Window *window;
	wl_list_for_each(window, &wm.windows, link) {
		if (window->space != space)
			continue;
		if (window->space->focused != NULL &&
				window->space->focused != window) {
			// HACK: Intentionally invalidate Rect to prevent rendering
			window->layout.width = window->layout.height = -1;
			continue;
		}
		if (!window->maximized) {
			river_window_v1_inform_maximized(window->obj);
			window->maximized = true;
		}
		window->layout.x = bounds.x + monocle_borderpx;
		window->layout.y = bounds.y + monocle_borderpx;
		window->layout.width = bounds.width - monocle_borderpx * 2;
		window->layout.height = bounds.height - monocle_borderpx * 2;
	}
}

extern void tiled_layout(struct Space *space, struct Rect bounds) {
	int count = 0, w = 0, rightwidth = bounds.width, rightheight = bounds.height;
	struct Window *window;
	wl_list_for_each(window, &wm.windows, link) {
		if (window->space == space)
			count++;
	}
	wl_list_for_each(window, &wm.windows, link) {
		if (window->space != space)
			continue;
		if (window->maximized) {
			river_window_v1_inform_unmaximized(window->obj);
			window->maximized = false;
		}
		if (count == 1 || w == 0) {
			// Left side "main" window
			float split = (count == 1 ? 1.0 : tiled_splitratio);
			window->layout.x = bounds.x + tiled_borderpx;
			window->layout.y = bounds.y + tiled_borderpx;
			window->layout.width = bounds.width * split - tiled_borderpx * 2;
			window->layout.height = bounds.height - tiled_borderpx * 2;

			rightwidth -= window->layout.width + tiled_borderpx;
		} else {
			// Right side "stacked" windows
			window->layout.x = bounds.x + bounds.width - rightwidth + tiled_borderpx;
			window->layout.y = bounds.y + bounds.height - rightheight + tiled_borderpx;
			window->layout.width = rightwidth - tiled_borderpx * 2;
			window->layout.height = rightheight / (count - w) - tiled_borderpx * 2;

			rightheight -= window->layout.height + tiled_borderpx;
		}
		w++;
	}
}


// Manage sequence/render sequence functions

// Perform actions for a Window that have been called for by some event, but
// must be done during the manage sequence
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
		struct Space *space = window->space;
		if (space->output != NULL &&
				space->output->active == space) {
			river_window_v1_inform_fullscreen(window->obj);
			river_window_v1_fullscreen(window->obj,
					space->output->obj);
		}
		window->fullscreen = false;
	}
	if (window->exit_fullscreen) {
		river_window_v1_exit_fullscreen(window->obj);
		river_window_v1_inform_not_fullscreen(window->obj);
		window->exit_fullscreen = false;
	}
}

// Per-Space focus is technically just internal bookkeeping; this function
// propagates the "real", per-Seat, focus state to the compositor during each
// manage sequence.
// Called at the end of the sequence so that other functions can modify focus
extern void seat_manage_focus(struct Seat *seat) {
	if (seat->focused->focused != NULL)
		river_seat_v1_focus_window(seat->obj, seat->focused->focused->obj);
	else
		river_seat_v1_clear_focus(seat->obj);
}

// Perform the main, per-Space, manage sequence logic
extern void manage_space(struct Space *space) {
	struct Output *output = active_on_output(space);
	if (output == NULL)
		return;

	space->layout(space, output->windowed);

	struct Window *window;
	wl_list_for_each(window, &wm.windows, link) {
		if (window->space != output->active || !valid_rect(window->layout))
			continue;
		river_window_v1_use_ssd(window->obj);
		river_window_v1_set_tiled(window->obj, 15);
		river_window_v1_propose_dimensions(window->obj,
				window->layout.width,
				window->layout.height);
	}
}

// Perform the main, per-Space, render sequence logic
extern void render_space(struct Space *space) {
	struct Output *output = active_on_output(space);
	if (output == NULL)
		return;

	struct Window *window;
	wl_list_for_each(window, &wm.windows, link) {
		if (window->space != space || !valid_rect(window->layout))
			continue;

		int borderpx	= tiled_borderpx;
		uint32_t *color	= border_color;
		if (space->layout == monocle_layout)
			borderpx = monocle_borderpx;
		if (window == window->space->focused) {
			river_node_v1_place_top(window->node);
			color = semi_focused_color;
		}
		river_window_v1_show(window->obj);
		river_window_v1_set_borders(window->obj, 15, borderpx,
				color[0], color[1], color[2], color[3]);
		river_node_v1_set_position(window->node,
				window->layout.x, window->layout.y);
	}
}

extern void seat_render_focus(struct Seat *seat) {
	if (seat->ls_focused || seat->focused->focused == NULL)
		return;
	int borderpx = tiled_borderpx;
	if (seat->focused->layout == monocle_layout)
		borderpx = monocle_borderpx;
	river_window_v1_set_borders(seat->focused->focused->obj, 15, borderpx,
			focused_color[0], focused_color[1], focused_color[2], focused_color[3]);
	river_node_v1_place_top(seat->focused->focused->node);
}
