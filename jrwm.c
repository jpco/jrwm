// jrwm.c -- Main server file for JrWM

// This file is meant to have as little "interesting" logic as possible;
// instead, it should only be responsible for handling the protocol layer,
// managing memory for the core data structures, and dispatching to functions in
// other, less boilerplate-burdened files whenever it can.

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

#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "jrwm.h"

struct WindowManager wm;

struct river_window_manager_v1 *window_manager_v1;
struct river_xkb_bindings_v1 *xkb_bindings_v1;
struct river_layer_shell_v1 *layer_shell_v1;


// Listeners and event handlers for the core types

static void output_handle_removed(void *data, struct river_output_v1 *obj) {
	struct Output *output = data;

	replace_output(output);
	river_layer_shell_output_v1_destroy(output->ls);
	river_output_v1_destroy(output->obj);
	wl_list_remove(&output->link);
	free(output);
}

static void output_handle_dimensions(void *data, struct river_output_v1 *obj, int32_t width, int32_t height) {}
static void output_handle_position(void *data, struct river_output_v1 *obj, int32_t x, int32_t y) {}
static void output_handle_wl_output(void *data, struct river_output_v1 *obj, uint32_t name) {}

const struct river_output_v1_listener river_output_listener = {
	.removed = output_handle_removed,
	.wl_output = output_handle_wl_output,
	.position = output_handle_position,
	.dimensions = output_handle_dimensions,
};

static void ls_output_handle_non_exclusive_area(void *data, struct river_layer_shell_output_v1 *river_ls_output_v1, int32_t x, int32_t y, int32_t width, int32_t height) {
	struct Output *output = data;
	output->windowed.x = x;
	output->windowed.y = y;
	output->windowed.width = width;
	output->windowed.height = height;
}

const struct river_layer_shell_output_v1_listener ls_output_listener = {
	.non_exclusive_area = ls_output_handle_non_exclusive_area
};

static void window_handle_closed(void *data, struct river_window_v1 *obj) {
	struct Window *window = data;

	replace_window(window);
	river_window_v1_destroy(window->obj);
	wl_list_remove(&window->link);
	free(window);
}

static void window_handle_fullscreen_requested(void *data, struct river_window_v1 *obj, struct river_output_v1 *river_output) {
	struct Window *window = data;
	window->fullscreen = true;
}

static void window_handle_exit_fullscreen_requested(void *data, struct river_window_v1 *obj) {
	struct Window *window = data;
	window->exit_fullscreen = true;
}

static void window_handle_dimensions(void *data, struct river_window_v1 *obj, int32_t width, int32_t height) {
	struct Window *window = data;
	window->layout.width = width;
	window->layout.height = height;
}

static void window_handle_maximize_requested(void *data, struct river_window_v1 *obj) {
	struct Window *window = data;
	if (window->space->layout != monocle_layout)
		window->space->layout = monocle_layout;
	window->space->focused = window;
}

static void window_handle_unmaximize_requested(void *data, struct river_window_v1 *obj) {
	struct Window *window = data;
	if (window->space->layout == monocle_layout)
		window->space->layout = tiled_layout;
}

// Ignored events
static void window_handle_app_id(void *data, struct river_window_v1 *obj, const char *app_id) {}
static void window_handle_decoration_hint(void *data, struct river_window_v1 *obj, uint32_t hint) {}
static void window_handle_dimensions_hint(void *data, struct river_window_v1 *obj, int32_t min_width, int32_t min_height, int32_t max_width, int32_t max_height) {}
static void window_handle_identifier(void *data, struct river_window_v1 *obj, const char *indentifier) {}
static void window_handle_minimize_requested(void *data, struct river_window_v1 *obj) {}
static void window_handle_parent(void *data, struct river_window_v1 *obj, struct river_window_v1 *parent) {}
static void window_handle_pointer_move_requested(void *data, struct river_window_v1 *obj, struct river_seat_v1 *river_seat) {}
static void window_handle_pointer_resize_requested(void *data, struct river_window_v1 *obj, struct river_seat_v1 *river_seat, uint32_t edges) {}
static void window_handle_presentation_hint(void *data, struct river_window_v1 *obj, uint32_t hint) {}
static void window_handle_show_window_menu_requested(void *data, struct river_window_v1 *obj, int32_t x, int32_t y) {}
static void window_handle_title(void *data, struct river_window_v1 *obj, const char *title) {}
static void window_handle_unreliable_pid(void *data, struct river_window_v1 *obj, int32_t unreliable_pid) {}

const struct river_window_v1_listener river_window_listener = {
	.closed = window_handle_closed,
	.dimensions_hint = window_handle_dimensions_hint,
	.dimensions = window_handle_dimensions,
	.app_id = window_handle_app_id,
	.title = window_handle_title,
	.parent = window_handle_parent,
	.decoration_hint = window_handle_decoration_hint,
	.pointer_move_requested = window_handle_pointer_move_requested,
	.pointer_resize_requested = window_handle_pointer_resize_requested,
	.show_window_menu_requested = window_handle_show_window_menu_requested,
	.maximize_requested = window_handle_maximize_requested,
	.unmaximize_requested = window_handle_unmaximize_requested,
	.fullscreen_requested = window_handle_fullscreen_requested,
	.exit_fullscreen_requested = window_handle_exit_fullscreen_requested,
	.minimize_requested = window_handle_minimize_requested,
	.unreliable_pid = window_handle_unreliable_pid,
	.presentation_hint = window_handle_presentation_hint,
	.identifier = window_handle_identifier,
};

static void seat_handle_removed(void *data, struct river_seat_v1 *obj) {
	struct Seat *seat = data;

	remove_xkb_bindings(seat);
	river_layer_shell_seat_v1_destroy(seat->ls);
	river_seat_v1_destroy(seat->obj);
	wl_list_remove(&seat->link);
	free(seat);
}

static void seat_handle_window_interaction(void *data, struct river_seat_v1 *obj, struct river_window_v1 *river_window) {
	struct Seat *seat = data;
	struct Window *window = river_window_v1_get_user_data(river_window);
	seat->focused = window->space;
	window->space->focused = window;
}

static void seat_handle_op_delta(void *data, struct river_seat_v1 *obj, int32_t dx, int32_t dy) {}
static void seat_handle_op_release(void *data, struct river_seat_v1 *obj) {}
static void seat_handle_pointer_enter(void *data, struct river_seat_v1 *obj, struct river_window_v1 *river_window) {}
static void seat_handle_pointer_leave(void *data, struct river_seat_v1 *obj) {}
static void seat_handle_pointer_position(void *data, struct river_seat_v1 *obj, int32_t x, int32_t y) {}
static void seat_handle_shell_surface_interaction(void *data, struct river_seat_v1 *obj, struct river_shell_surface_v1 *river_shell_surface) {}
static void seat_handle_wl_seat(void *data, struct river_seat_v1 *obj, uint32_t id) {}

const struct river_seat_v1_listener river_seat_listener = {
	.removed = seat_handle_removed,
	.wl_seat = seat_handle_wl_seat,
	.pointer_enter = seat_handle_pointer_enter,
	.pointer_leave = seat_handle_pointer_leave,
	.window_interaction = seat_handle_window_interaction,
	.shell_surface_interaction = seat_handle_shell_surface_interaction,
	.op_delta = seat_handle_op_delta,
	.op_release = seat_handle_op_release,
	.pointer_position = seat_handle_pointer_position,
};

static void ls_seat_handle_focus_exclusive(void *data, struct river_layer_shell_seat_v1 *ls_seat) {
	struct Seat *seat = data;
	seat->ls_focused = true;
}
static void ls_seat_handle_focus_non_exclusive(void *data, struct river_layer_shell_seat_v1 *ls_seat) {
	struct Seat *seat = data;
	seat->ls_focused = true;
}
static void ls_seat_handle_focus_none(void *data, struct river_layer_shell_seat_v1 *ls_seat) {
	struct Seat *seat = data;
	seat->ls_focused = false;
}

struct river_layer_shell_seat_v1_listener ls_seat_listener = {
	.focus_exclusive = ls_seat_handle_focus_exclusive,
	.focus_non_exclusive = ls_seat_handle_focus_non_exclusive,
	.focus_none = ls_seat_handle_focus_none,
};


// WM event handlers and core type creation routines.

static void wm_handle_output(void *data, struct river_window_manager_v1 *obj, struct river_output_v1 *river_output) {
	struct Output *output = calloc(1, sizeof(struct Output));
	output->obj = river_output;
	output->ls = river_layer_shell_v1_get_output(layer_shell_v1, output->obj);

	place_output(output);
	river_output_v1_add_listener(output->obj, &river_output_listener, output);
	river_layer_shell_output_v1_add_listener(output->ls, &ls_output_listener, output);
	wl_list_insert(&wm.outputs, &output->link);
}

static void wm_handle_seat(void *data, struct river_window_manager_v1 *obj, struct river_seat_v1 *river_seat) {
	struct Seat *seat = calloc(1, sizeof(struct Seat));
	seat->obj = river_seat;
	seat->ls = river_layer_shell_v1_get_seat(layer_shell_v1, seat->obj);

	wl_list_init(&seat->xkb_bindings);
	init_xkb_bindings(seat);

	place_seat(seat);
	river_seat_v1_add_listener(seat->obj, &river_seat_listener, seat);
	river_layer_shell_seat_v1_add_listener(seat->ls, &ls_seat_listener, seat);
	wl_list_insert(&wm.seats, &seat->link);
}

static void wm_handle_window(void *data, struct river_window_manager_v1 *obj, struct river_window_v1 *river_window) {
	struct Window *window = calloc(1, sizeof(struct Window));
	window->obj = river_window;
	window->node = river_window_v1_get_node(window->obj);
	window->set_capabilities = true;

	place_window(window);
	river_window_v1_add_listener(window->obj, &river_window_listener, window);
	wl_list_insert(&wm.windows, &window->link);
}

static void wm_handle_manage_start(void *data, struct river_window_manager_v1 *obj) {
	struct Window *window;
	wl_list_for_each(window, &wm.windows, link)
		window_do_deferred(window);

	struct Space *space;
	wl_list_for_each(space, &wm.spaces, link)
		manage_space(space);

	struct Seat *seat;
	wl_list_for_each(seat, &wm.seats, link) {
		enable_xkb_bindings(seat);
		seat_manage_focus(seat);
	}
	river_window_manager_v1_manage_finish(window_manager_v1);
}

static void wm_handle_render_start(void *data, struct river_window_manager_v1 *window_manager_v1) {
	struct Window *window;
	wl_list_for_each(window, &wm.windows, link)
		river_window_v1_hide(window->obj);

	struct Space *space;
	wl_list_for_each(space, &wm.spaces, link)
		render_space(space);

	// struct Seat *seat;
	// wl_list_for_each(seat, &wm.seats, link)
	// 	seat_render_focus(seat);

	river_window_manager_v1_render_finish(window_manager_v1);
}

static void wm_handle_finished(void *data, struct river_window_manager_v1 *obj) {
	river_window_manager_v1_destroy(window_manager_v1);
	exit(0);
}

static void wm_handle_unavailable(void *data, struct river_window_manager_v1 *obj) {
	// river_window_manager_v1_destroy(obj); ?
	exit(1);
}

// Ignored WM events
static void wm_handle_session_locked(void *data, struct river_window_manager_v1 *obj) {}
static void wm_handle_session_unlocked(void *data, struct river_window_manager_v1 *obj) {}

static const struct river_window_manager_v1_listener wm_listener = {
	.unavailable = wm_handle_unavailable,
	.finished = wm_handle_finished,
	.manage_start = wm_handle_manage_start,
	.render_start = wm_handle_render_start,
	.session_locked = wm_handle_session_locked,
	.session_unlocked = wm_handle_session_unlocked,
	.window = wm_handle_window,
	.output = wm_handle_output,
	.seat = wm_handle_seat,
};

static void wm_init(void) {
	wl_list_init(&wm.outputs);
	wl_list_init(&wm.windows);
	wl_list_init(&wm.seats);
	wl_list_init(&wm.spaces);

	// Create our spaces
	// TODO: Move me out
	int i;
	for (i = 1; i <= 9; i++) {
		struct Space *space = calloc(1, sizeof(struct Space));
		space->layout = tiled_layout;
		wl_list_insert(&wm.spaces, &space->link);
	}
}


// Global gunk

static void handle_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
	if (strcmp(interface, river_window_manager_v1_interface.name) == 0 && version >= 4) {
		window_manager_v1 = wl_registry_bind(registry, name, &river_window_manager_v1_interface, 4);
	} else if (strcmp(interface, river_xkb_bindings_v1_interface.name) == 0) {
		xkb_bindings_v1 = wl_registry_bind(registry, name, &river_xkb_bindings_v1_interface, 1);
	} else if (strcmp(interface, river_layer_shell_v1_interface.name) == 0) {
		layer_shell_v1 = wl_registry_bind(registry, name, &river_layer_shell_v1_interface, 1);
	}
}

static void handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {}

static const struct wl_registry_listener registry_listener = {
	.global = handle_global,
	.global_remove = handle_global_remove,
};

int main(void) {
	struct wl_display *display = wl_display_connect(NULL);
	if (display == NULL) {
		fprintf(stderr, "failed to connect to Wayland server\n");
		return 1;
	}

	// Ensure children are automatically reaped
	signal(SIGCHLD, SIG_IGN);

	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	if (wl_display_roundtrip(display) < 0) {
		fprintf(stderr, "roundtrip failed\n");
		return 1;
	}

	if (window_manager_v1 == NULL || xkb_bindings_v1 == NULL || layer_shell_v1 == NULL) {
		fprintf(stderr, "required protocol not supported by the server\n");
		return 1;
	}

	wm_init();
	river_window_manager_v1_add_listener(window_manager_v1, &wm_listener, NULL);

	while (true) {
		if (wl_display_dispatch(display) < 0) {
			fprintf(stderr, "dispatch failed\n");
			return 1;
		}
	}
}
