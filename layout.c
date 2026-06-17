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

static int monocle_borderpx	=  0;

static int tiled_borderpx	=  2;
static int tiled_margin		= -2;	// Space between windows
static int tiled_output_padding	=  0;	// Space around windows
static float tiled_splitratio	=  0.52;

static bool pointer_follows_focus = false;


// Private functions for window management and rendering

static bool valid_rect(struct Rect r) {
	return r.width >= 0 && r.height >= 0;
}

// In-place shrink the boundaries of a Rect to accommodate the given border radius
static void subtract_border(struct Rect *rect, int32_t border) {
	rect->x = rect->x + border;
	rect->y = rect->y + border;
	rect->width  = (rect->width  - border * 2 < 0) ? 0 : rect->width  - border * 2;
	rect->height = (rect->height - border * 2 < 0) ? 0 : rect->height - border * 2;
}

static void render_border(struct Window *window, int thickness, uint32_t *color) {
	river_window_v1_set_borders(window->obj, 15, thickness,
			color[0], color[1], color[2], color[3]);
}

static void unfullscreen_window(struct Window *window) {
	river_window_v1_exit_fullscreen(window->obj);
	river_window_v1_inform_not_fullscreen(window->obj);
	window->fullscreen = false;
}


// Handle creation/deletion of the core objects that all point to each other.
//  - These run before the relevant wl_list in wm is updated
//  - None of these run during a manage or render sequence

// Find a Space for this Output to activate
extern void place_output(struct Output *output) {
	struct Space *space;

	// If we have a focused, inactive Space, use that
	struct Seat *seat;
	wl_list_for_each(seat, &wm.seats, link) {
		space = seat->focused;
		if (space->output == NULL || space->output->active != space) {
			output->active = seat->focused;
			seat->focused->output = output;
			return;
		}
	}

	// Otherwise, just pick the first inactive Space
	wl_list_for_each(space, &wm.spaces, link) {
		if (space->output == NULL || space->output->active != space) {
			output->active = space;
			space->output = output;
			return;
		}
	}

	// Fallback: make a new space to use!  This may cause problems
	// with the "nth-space" bindings, but it's better than segfaults.
	space = create_space();
	output->active = space;
	space->output = output;
	wl_list_insert(&wm.spaces, &space->link);
}

// Replace this Output with any other where necessary
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
	// If there is one, pick the first Seat's focused Space
	// TODO: Pick the correct Seat, once we have a way to know what that is
	if (!wl_list_empty(&wm.seats)) {
		struct Seat *seat = wl_container_of(wm.seats.next, seat, link);
		window->space = seat->focused;
		seat->focused->focused = window;
	}

	// Fallback: just pick the first Space
	if (window->space == NULL) {
		window->space = wl_container_of(wm.spaces.next, window->space, link);
		window->space->focused = window;
	}
}

// Replace this Window with any other where necessary
extern void replace_window(struct Window *window) {
	struct Seat *seat;
	// Unclear if this is even possible, but let's play it safe
	wl_list_for_each(seat, &wm.seats, link)
		if (seat->entered == window)
			seat->entered = NULL;

	if (window->space->focused != window)
		return;
	struct Window *r, *replacement = NULL;
	// Focus the first Window in the Space (or NULL, if no other Window)
	wl_list_for_each(r, &wm.windows, link) {
		if (r->space == window->space && r != window) {
			replacement = r;
			break;
		}
	}
	window->space->focused = replacement;
}

// Find a Space for this Seat to focus on
extern void place_seat(struct Seat *seat) {
	seat->focused = wl_container_of(wm.spaces.next, seat->focused, link);
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
		subtract_border(&bounds, monocle_borderpx);
		window->layout = bounds;
	}
}

extern void tiled_layout(struct Space *space, struct Rect bounds) {
	subtract_border(&bounds, tiled_output_padding);
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
			window->layout = bounds;
			if (count > 1)
				window->layout.width *= tiled_splitratio;

			rightwidth -= window->layout.width + tiled_margin;
		} else {
			// Right side "stacked" windows
			window->layout.x = bounds.x + bounds.width - rightwidth;
			window->layout.y = bounds.y + bounds.height - rightheight;
			window->layout.width = rightwidth;
			window->layout.height = rightheight / (count - w);

			rightheight -= window->layout.height + tiled_margin;
		}
		subtract_border(&window->layout, tiled_borderpx);
		w++;
	}
}


// Manage sequence/render sequence functions

// Perform actions for a Window that have been called for by some event, but
// must be done during the manage sequence
extern void manage_window_deferred(struct Window *window) {
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
	if (window->enter_fullscreen) {
		struct Space *space = window->space;
		if (space->output != NULL &&
				space->output->active == space) {
			river_window_v1_inform_fullscreen(window->obj);
			river_window_v1_fullscreen(window->obj,
					space->output->obj);
			window->fullscreen = true;
		}
		window->enter_fullscreen = false;
	}
	if (window->exit_fullscreen) {
		unfullscreen_window(window);
		window->exit_fullscreen = false;
	}
}

// Per-Space focus is technically just internal bookkeeping; this function
// propagates the "real", per-Seat, focus state to the compositor during each
// manage sequence.
// Called at the end of the sequence so that other functions can modify focus
extern void manage_seat_focus(struct Seat *seat) {
	// Change focus, if necessary
	if (seat->moved && seat->entered != NULL) {
		seat->focused = seat->entered->space;
		seat->focused->focused = seat->entered;
	}

	// Propagate focus information to River
	if (seat->focused->output != NULL)
		river_layer_shell_output_v1_set_default(seat->focused->output->ls);
	if (seat->focused->focused != NULL)
		river_seat_v1_focus_window(seat->obj, seat->focused->focused->obj);
	else
		river_seat_v1_clear_focus(seat->obj);

	// Un-fullscreen if there's a layer shell focused
	if (seat->focused->focused != NULL
			&& seat->focused->focused->fullscreen
			&& seat->ls_focused) {
		unfullscreen_window(seat->focused->focused);
	}

	// Warp the pointer to the focused window
	if (pointer_follows_focus && seat->warp && !seat->ls_focused) {
		struct Window *window = seat->focused->focused;
		if (window != NULL) {
			int32_t x = window->layout.x + window->layout.width/2;
			int32_t y = window->layout.y + window->layout.height/2;
			river_seat_v1_pointer_warp(seat->obj, x, y);
		}
	}
	seat->warp = false;
	seat->moved = false;
	seat->entered = NULL;
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
		if (window->fullscreen && window->space->focused != window)
			unfullscreen_window(window);
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
		river_window_v1_show(window->obj);
		river_node_v1_set_position(window->node,
				window->layout.x, window->layout.y);
		if (space->layout == monocle_layout)
			render_border(window, monocle_borderpx, border_color);
		else
			render_border(window, tiled_borderpx, border_color);
	}
}

extern void render_seat_focus(struct Seat *seat) {
	struct Window *window = seat->focused->focused;
	if (window == NULL || seat->ls_focused)
		return;
	river_node_v1_place_top(window->node);
	if (seat->focused->layout == monocle_layout)
		render_border(window, monocle_borderpx, focused_color);
	else
		render_border(window, tiled_borderpx, focused_color);
}
