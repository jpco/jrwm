// Copyright 2026 Isaac Freund, 2026 Jack Conger.  All rights reserved.

// This program is free software: you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with
// this program. If not, see <https://www.gnu.org/licenses/>.

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include <river-layer-shell-v1.h>
#include <river-window-management-v1.h>
#include <river-xkb-bindings-v1.h>


// Types and globals

struct Rect {
	int32_t x, y, width, height;
};

// A Space is an abstract collection of Windows on an Output.
// The Windows may not exist.  The Output may not either.
// But we need a valid Space for nearly everything in the WM to point at.
struct Space {
	struct wl_list link; // WindowManager.spaces
	struct Output *output;
	struct Window *focused;

	struct Window *fullscreen;  // These two are usually null
	struct Window *maximized;   // and mostly just impact layout
};

// An Output is like an actual physical display.
struct Output {
	struct wl_list link; // WindowManager.outputs
	struct river_output_v1 *obj;
	struct river_layer_shell_output_v1 *ls;

	struct Rect windowed;

	struct Space *active;
};

// A Window is a rectangle under management.
struct Window {
	struct wl_list link; // WindowManager.windows
	struct river_window_v1 *obj;
	struct river_node_v1 *node;

	// tasks for the manage sequence
	bool set_capabilities; // window_v1.set_capabilities
	bool close;            // window_v1.close
	bool fullscreen;       // window_v1.inform_fullscreen
	bool exit_fullscreen;  // window_v1.inform_not_fullscreen
	bool maximize;         // window_v1.inform_maximized
	bool unmaximize;       // window_v1.inform_unmaximized

	// information for render sequence
	struct Rect layout;

	struct Window *parent; // usually null
	struct Space *space;
};

// A Seat is a collection of input devices.
struct Seat {
	struct wl_list link; // WindowManager.seats
	struct river_seat_v1 *obj;
	struct river_layer_shell_seat_v1 *ls;
	struct wl_list xkb_bindings;  // XkbBinding

	struct Space *focused;
};

struct XkbBinding {
	struct wl_list link; // Seat.xkb_bindings
	struct river_xkb_binding_v1 *obj;
	struct Seat *seat;

	void (*dispatch)(struct Seat *, char **argv);
	char **argv;
	bool enable;  // Enable this binding on the next window_manage
};

struct WindowManager {
	struct wl_list outputs; // Output
	struct wl_list windows; // Window
	struct wl_list seats;   // Seat
	struct wl_list spaces;  // Space
};

struct WindowManager wm;

struct river_window_manager_v1 *window_manager_v1;
struct river_xkb_bindings_v1 *xkb_bindings_v1;
struct river_layer_shell_v1 *layer_shell_v1;

static bool debug = false;


// Listeners and event handlers for the core types

static void output_handle_removed(void *data, struct river_output_v1 *obj) {
	if (debug) fprintf(stderr, "Handling removed output\n");
	struct Output *output = data;

	struct Output *replacement = NULL, *r;
	wl_list_for_each(r, &wm.outputs, link)
		if (r != output)
			replacement = r;

	struct Space *space;
	wl_list_for_each(space, &wm.spaces, link)
		if (space->output == output)
			space->output = replacement;

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
	if (debug) fprintf(stderr, "Handling non-exclusive area\n");
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
	if (debug) fprintf(stderr, "Handling closed window\n");
	struct Window *window = data;

	struct Space *space;
	wl_list_for_each(space, &wm.spaces, link) {
		if (space->maximized == window)
			space->maximized = NULL;
		if (space->fullscreen == window)
			space->fullscreen = NULL;
		if (space->focused == window) {
			struct Window *r, *replacement = NULL;
			wl_list_for_each(r, &wm.windows, link) {
				if (r->space == space && r != window)
					replacement = r;
			}
			space->focused = replacement;
		}
	}

	river_window_v1_destroy(window->obj);
	wl_list_remove(&window->link);
	free(window);
}

static void window_handle_fullscreen_requested(void *data, struct river_window_v1 *obj, struct river_output_v1 *river_output) {
	if (debug) fprintf(stderr, "Handling fullscreen request\n");
	struct Window *window = data;
	if (window->space->fullscreen == NULL) {
		window->space->fullscreen = window;
		window->fullscreen = true;
	}
}

static void window_handle_exit_fullscreen_requested(void *data, struct river_window_v1 *obj) {
	if (debug) fprintf(stderr, "Handling exit-fullscreen request\n");
	struct Window *window = data;
	if (window->space->fullscreen == window) {
		window->space->fullscreen = NULL;
		window->exit_fullscreen = true;
	}
}

static void window_handle_maximize_requested(void *data, struct river_window_v1 *obj) {
	if (debug) fprintf(stderr, "Handling maximize request\n");
	struct Window *window = data;
	if (window->space->maximized == NULL) {
		window->space->maximized = window;
		window->maximize = true;
	}
}

static void window_handle_unmaximize_requested(void *data, struct river_window_v1 *obj) {
	if (debug) fprintf(stderr, "Handling unmaximize request\n");
	struct Window *window = data;
	if (window->space->maximized == window) {
		window->space->maximized = NULL;
		window->unmaximize = true;
	}
}

static void window_handle_parent(void *data, struct river_window_v1 *obj, struct river_window_v1 *parent) {
	struct Window *window = data;
	if (parent != NULL) {
		if (debug) fprintf(stderr, "Handling non-null parent window\n");
		window->parent = river_window_v1_get_user_data(parent);
	} else {
		if (debug) fprintf(stderr, "Handling null parent window\n");
		window->parent = NULL;
	}
}

static void window_handle_dimensions(void *data, struct river_window_v1 *obj, int32_t width, int32_t height) {
	if (debug) fprintf(stderr, "Handling window dimensions\n");
	struct Window *window = data;
	window->layout.width = width;
	window->layout.height = height;
}

// Ignored events
static void window_handle_app_id(void *data, struct river_window_v1 *obj, const char *app_id) {}
static void window_handle_decoration_hint(void *data, struct river_window_v1 *obj, uint32_t hint) {}
static void window_handle_dimensions_hint(void *data, struct river_window_v1 *obj, int32_t min_width, int32_t min_height, int32_t max_width, int32_t max_height) {}
static void window_handle_identifier(void *data, struct river_window_v1 *obj, const char *indentifier) {}
static void window_handle_minimize_requested(void *data, struct river_window_v1 *obj) {}
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
	if (debug) fprintf(stderr, "Handling removed seat\n");
	struct Seat *seat = data;

	struct XkbBinding *binding, *tmp;
	wl_list_for_each_safe(binding, tmp, &seat->xkb_bindings, link) {
		river_xkb_binding_v1_destroy(binding->obj);
		wl_list_remove(&binding->link);
		free(binding);
	}

	river_layer_shell_seat_v1_destroy(seat->ls);
	river_seat_v1_destroy(seat->obj);
	wl_list_remove(&seat->link);
	free(seat);
}

static void seat_handle_window_interaction(void *data, struct river_seat_v1 *obj, struct river_window_v1 *river_window) {
	if (debug) fprintf(stderr, "Handling window interaction\n");
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

static void ls_seat_handle_focus_exclusive(void *data, struct river_layer_shell_seat_v1 *ls_seat) {}
static void ls_seat_handle_focus_non_exclusive(void *data, struct river_layer_shell_seat_v1 *ls_seat) {}
static void ls_seat_handle_focus_none(void *data, struct river_layer_shell_seat_v1 *ls_seat) {}

struct river_layer_shell_seat_v1_listener ls_seat_listener = {
	.focus_exclusive = ls_seat_handle_focus_exclusive,
	.focus_non_exclusive = ls_seat_handle_focus_non_exclusive,
	.focus_none = ls_seat_handle_focus_none,
};


// Key bindings (this could/should? be moved to a different file)

static void xkb_binding_handle_pressed(void *data, struct river_xkb_binding_v1 *obj) {
	if (debug) fprintf(stderr, "Handling xkb binding\n");
	struct XkbBinding *binding = data;
	binding->dispatch(binding->seat, binding->argv);
}

static void xkb_binding_handle_released(void *data, struct river_xkb_binding_v1 *obj) {}

const struct river_xkb_binding_v1_listener river_xkb_binding_listener = {
	.pressed = xkb_binding_handle_pressed,
	.released = xkb_binding_handle_released,
};

static void xkb_binding_create(struct Seat *seat, uint32_t mods, xkb_keysym_t keysym, void (*dispatch)(struct Seat *seat, char **argv), char **argv) {
	struct XkbBinding *binding = calloc(1, sizeof(struct XkbBinding));
	binding->obj = river_xkb_bindings_v1_get_xkb_binding(xkb_bindings_v1, seat->obj, keysym, mods);
	binding->seat = seat;
	binding->dispatch = dispatch;
	binding->argv = argv;
	binding->enable = true;

	river_xkb_binding_v1_add_listener(binding->obj, &river_xkb_binding_listener, binding);
	wl_list_insert(&seat->xkb_bindings, &binding->link);
}

void binding_spawn(struct Seat *seat, char **argv) {
	if (fork() == 0) {
		setsid();
		signal(SIGCHLD, SIG_DFL);
		execvp((const char*)argv[0], (char *const *)argv);
		exit(12);  // Just in case
	}
}

void binding_exit(struct Seat *seat, char **argv) {
	river_window_manager_v1_exit_session(window_manager_v1);
}

void binding_close(struct Seat *seat, char **argv) {
	if (seat->focused->focused != NULL)
		seat->focused->focused->close = true;
}

void binding_focus_next(struct Seat *seat, char **argv) {
	struct Space *space = seat->focused;
	if (space->focused == NULL)
		return;
	if (space->maximized != NULL || space->fullscreen != NULL)
		return;
	bool next = false, first = true;
	struct Window *w = NULL, *fw = NULL;
	wl_list_for_each(w, &wm.windows, link) {
		if (w->space != space)
			continue;
		if (first) {
			fw = w;
			first = false;
		}
		if (next) {
			space->focused = w;
			return;
		}
		if (w == space->focused)
			next = true;
	}
	space->focused = fw;
}

void binding_focus_prev(struct Seat *seat, char **argv) {
	struct Space *space = seat->focused;
	if (space->focused == NULL)
		return;
	if (space->maximized != NULL || space->fullscreen != NULL)
		return;
	bool next = false, first = true;
	struct Window *w = NULL, *fw = NULL;
	wl_list_for_each_reverse(w, &wm.windows, link) {
		if (w->space != space)
			continue;
		if (first) {
			fw = w;
			first = false;
		}
		if (next) {
			space->focused = w;
			return;
		}
		if (w == space->focused)
			next = true;
	}
	space->focused = fw;
}

void binding_move_next(struct Seat *seat, char **argv) {
	struct Space *space = seat->focused;
	if (space->focused == NULL || space->focused->parent != NULL)
		return;
	bool next = false;
	struct Window *curr = space->focused, *target = NULL, *w = NULL;
	wl_list_for_each(w, &wm.windows, link) {
		if (w->space != space || w->parent != NULL)
			continue;
		if (next) {
			target = w;
			break;
		}
		if (w == curr)
			next = true;
	}
	wl_list_remove(&curr->link);
	if (target != NULL) {
		wl_list_insert(&target->link, &curr->link);
	} else {
		wl_list_insert(&wm.windows, &curr->link);
	}
}

void binding_move_prev(struct Seat *seat, char **argv) {
	struct Space *space = seat->focused;
	if (space->focused == NULL || space->focused->parent != NULL)
		return;
	bool next = false;
	struct Window *curr = space->focused, *target = NULL, *w = NULL;
	wl_list_for_each_reverse(w, &wm.windows, link) {
		if (w->space != space || w->parent != NULL)
			continue;
		if (next) {
			target = w;
			break;
		}
		if (w == curr)
			next = true;
	}
	wl_list_remove(&curr->link);
	if (target != NULL) {
		wl_list_insert(target->link.prev, &curr->link);
	} else {
		wl_list_insert(wm.windows.prev, &curr->link);
	}
}

void binding_toggle_maximize(struct Seat *seat, char **argv) {
	struct Space *space = seat->focused;
	if (space->focused == NULL)
		return;
	if (space->maximized != NULL && space->maximized != space->focused)
		return;
	if (space->maximized == NULL) {
		space->maximized = space->focused;
		space->focused->maximize = true;
	} else {
		space->maximized = NULL;
		space->focused->unmaximize = true;
	}
}

void binding_switch_space(struct Seat *seat, char **argv) {
	struct Space *space = seat->focused, *s, *unfocused = NULL;
	wl_list_for_each(s, &wm.spaces, link) {
		if (s != space)
			unfocused = s;
	}
	unfocused->output = seat->focused->output;
	unfocused->output->active = unfocused;
	seat->focused = unfocused;
}

static char *spawn_foot[]	= {"footclient", NULL};
static char *spawn_rofi[]	= {"rofi", "-show", "combi", NULL};

static char *spawn_mute[]	= {"mediactl", "pamixer", "--mute", "--set-volume", "0", NULL};
static char *spawn_volume_up[]	= {"mediactl", "pamixer", "--unmute", "--increase", "5", NULL};
static char *spawn_volume_down[]	= {"mediactl", "pamixer", "--decrease", "5", NULL};
static char *spawn_brightness_up[]	= {"mediactl", "brightnessctl", "-e", "set", "5%+", NULL};
static char *spawn_brightness_down[]	= {"mediactl", "brightnessctl", "-e", "set", "5%-", NULL};

static void init_xkb_bindings(struct Seat *seat) {
	xkb_binding_create(seat, RIVER_SEAT_V1_MODIFIERS_MOD4, XKB_KEY_Return, binding_spawn, spawn_foot);
	xkb_binding_create(seat, RIVER_SEAT_V1_MODIFIERS_MOD4, XKB_KEY_space, binding_spawn, spawn_rofi);
	xkb_binding_create(seat, RIVER_SEAT_V1_MODIFIERS_MOD4, XKB_KEY_q, binding_close, NULL);
	xkb_binding_create(seat, RIVER_SEAT_V1_MODIFIERS_MOD4, XKB_KEY_Escape, binding_exit, NULL);
	xkb_binding_create(seat, RIVER_SEAT_V1_MODIFIERS_MOD4, XKB_KEY_j, binding_focus_next, NULL);
	xkb_binding_create(seat, RIVER_SEAT_V1_MODIFIERS_MOD4, XKB_KEY_k, binding_focus_prev, NULL);
	xkb_binding_create(seat, RIVER_SEAT_V1_MODIFIERS_MOD4|RIVER_SEAT_V1_MODIFIERS_SHIFT, XKB_KEY_j, binding_move_next, NULL);
	xkb_binding_create(seat, RIVER_SEAT_V1_MODIFIERS_MOD4|RIVER_SEAT_V1_MODIFIERS_SHIFT, XKB_KEY_k, binding_move_prev, NULL);
	xkb_binding_create(seat, RIVER_SEAT_V1_MODIFIERS_MOD4, XKB_KEY_m, binding_toggle_maximize, NULL);
	xkb_binding_create(seat, RIVER_SEAT_V1_MODIFIERS_MOD4, XKB_KEY_s, binding_switch_space, NULL);

	xkb_binding_create(seat, RIVER_SEAT_V1_MODIFIERS_NONE, XKB_KEY_XF86AudioMute, binding_spawn, spawn_mute);
	xkb_binding_create(seat, RIVER_SEAT_V1_MODIFIERS_NONE, XKB_KEY_XF86AudioRaiseVolume, binding_spawn, spawn_volume_up);
	xkb_binding_create(seat, RIVER_SEAT_V1_MODIFIERS_NONE, XKB_KEY_XF86AudioLowerVolume, binding_spawn, spawn_volume_down);
	xkb_binding_create(seat, RIVER_SEAT_V1_MODIFIERS_NONE, XKB_KEY_XF86MonBrightnessUp, binding_spawn, spawn_brightness_up);
	xkb_binding_create(seat, RIVER_SEAT_V1_MODIFIERS_NONE, XKB_KEY_XF86MonBrightnessDown, binding_spawn, spawn_brightness_down);
}


// WM event handlers and core type creation routines.

static void wm_handle_output(void *data, struct river_window_manager_v1 *obj, struct river_output_v1 *river_output) {
	if (debug) fprintf(stderr, "Handling new output\n");
	struct Output *output = calloc(1, sizeof(struct Output));
	output->obj = river_output;
	output->ls = river_layer_shell_v1_get_output(layer_shell_v1, output->obj);

	struct Space *space;
	wl_list_for_each(space, &wm.spaces, link) {
		output->active = space;
		if (space->output == NULL)
			space->output = output;
	}

	river_output_v1_add_listener(output->obj, &river_output_listener, output);
	river_layer_shell_output_v1_add_listener(output->ls, &ls_output_listener, output);
	wl_list_insert(&wm.outputs, &output->link);
}

static void wm_handle_seat(void *data, struct river_window_manager_v1 *obj, struct river_seat_v1 *river_seat) {
	if (debug) fprintf(stderr, "Handling new seat\n");
	struct Seat *seat = calloc(1, sizeof(struct Seat));
	seat->obj = river_seat;
	seat->ls = river_layer_shell_v1_get_seat(layer_shell_v1, seat->obj);

	wl_list_init(&seat->xkb_bindings);
	init_xkb_bindings(seat);

	struct Space *space;
	wl_list_for_each(space, &wm.spaces, link) {
		seat->focused = space;
	}

	river_seat_v1_add_listener(seat->obj, &river_seat_listener, seat);
	river_layer_shell_seat_v1_add_listener(seat->ls, &ls_seat_listener, seat);
	wl_list_insert(&wm.seats, &seat->link);
}

static void wm_handle_window(void *data, struct river_window_manager_v1 *obj, struct river_window_v1 *river_window) {
	if (debug) fprintf(stderr, "Handling new window\n");
	struct Window *window = calloc(1, sizeof(struct Window));
	window->obj = river_window;
	window->node = river_window_v1_get_node(window->obj);
	window->set_capabilities = true;

	struct Seat *seat;
	wl_list_for_each(seat, &wm.seats, link) {
		struct Space *space = seat->focused;
		window->space = space;
		if (space->maximized == NULL && space->fullscreen == NULL)
			space->focused = window;
	}

	river_window_v1_add_listener(window->obj, &river_window_listener, window);
	wl_list_insert(&wm.windows, &window->link);
}


// The main WM loops, which reflect in-memory state out to the compositor.

static int borderpx = 2;
static float splitratio = 0.52;

static void layout_space(struct Space *space, struct Rect bounds) {
	int count = 0, w = 0, rightwidth = bounds.width, rightheight = bounds.height;
	struct Window *window;
	wl_list_for_each(window, &wm.windows, link) {
		if (window->space == space && window->parent == NULL)
			count++;
	}
	wl_list_for_each(window, &wm.windows, link) {
		if (window->space != space || window->parent != NULL)
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

static void place_child_window(struct Window *window, struct Rect bounds) {
	struct Window *parent = window;
	while (parent->parent != NULL)
		parent = parent->parent;

	int32_t center_x = parent->layout.x + parent->layout.width / 2;
	int32_t center_y = parent->layout.y + parent->layout.height / 2;
	window->layout.x = center_x - window->layout.width / 2;
	window->layout.y = center_y - window->layout.height / 2;
	if (window->layout.x + window->layout.width > bounds.width)
		window->layout.x = bounds.width - window->layout.width;
	if (window->layout.y + window->layout.height > bounds.height)
		window->layout.y = bounds.height - window->layout.height;

	// TODO: limit the window size
	if (window->layout.x < bounds.x)
		window->layout.x = bounds.x;
	if (window->layout.y < bounds.y)
		window->layout.y = bounds.y;
}

static bool valid_rect(struct Rect r) {
	return r.width >= 0 && r.height >= 0;
}

static void wm_handle_manage_start(void *data, struct river_window_manager_v1 *obj) {
	if (debug)
		fprintf(stderr, "MANAGE [seats: %d; outputs: %d; spaces: %d; windows: %d]\n",
			wl_list_length(&wm.seats),
			wl_list_length(&wm.outputs),
			wl_list_length(&wm.spaces),
			wl_list_length(&wm.windows));

	struct Window *window;
	struct Output *output;
	struct Seat *seat;

	// Perform delayed actions which can only be done during the manage sequence
	wl_list_for_each(window, &wm.windows, link) {
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
	wl_list_for_each(seat, &wm.seats, link) {
		struct XkbBinding *binding;
		wl_list_for_each(binding, &seat->xkb_bindings, link) {
			if (binding->enable) {
				river_xkb_binding_v1_enable(binding->obj);
				binding->enable = false;
			}
		}
	}

	// Perform window management
	wl_list_for_each(output, &wm.outputs, link) {
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
					if (window->parent != NULL) {
						river_window_v1_use_csd(window->obj);
						river_window_v1_set_tiled(window->obj, 0);
						river_window_v1_set_dimension_bounds(window->obj,
								output->windowed.width,
								output->windowed.height);
						river_window_v1_propose_dimensions(window->obj,
								0,
								0);
						continue;
					}
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
	wl_list_for_each(seat, &wm.seats, link) {
		if (seat->focused->focused != NULL)
			river_seat_v1_focus_window(seat->obj, seat->focused->focused->obj);
		else
			river_seat_v1_clear_focus(seat->obj);
	}

	river_window_manager_v1_manage_finish(window_manager_v1);
}

static void wm_handle_render_start(void *data, struct river_window_manager_v1 *window_manager_v1) {
	if (debug)
		fprintf(stderr, "RENDER [seats: %d; outputs: %d; spaces: %d; windows: %d]\n",
			wl_list_length(&wm.seats),
			wl_list_length(&wm.outputs),
			wl_list_length(&wm.spaces),
			wl_list_length(&wm.windows));

	struct Output *output;
	wl_list_for_each(output, &wm.outputs, link) {
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
					continue;
				} else if (window->parent == NULL) {
					river_window_v1_hide(window->obj);
					continue;
				}
			}
			river_window_v1_show(window->obj);
			if (window->parent != NULL) {
				place_child_window(window, output->windowed);
				river_node_v1_place_top(window->node);
				river_window_v1_set_borders(window->obj, 15, 0, 0, 0, 0, 0);
			} else if (window == window->space->focused) {
				river_node_v1_place_top(window->node);
				river_window_v1_set_borders(window->obj, 15, 2,
						0x77777777,
						0xAAAAAAAA,
						0x99999999,
						0xFFFFFFFF);
			} else {
				river_window_v1_set_borders(window->obj, 15, 2,
						0x33333333,
						0x33333333,
						0x33333333,
						0xFFFFFFFF);
			}
			river_node_v1_set_position(window->node,
					window->layout.x,
					window->layout.y);
		}
	}
	river_window_manager_v1_render_finish(window_manager_v1);
}


// Reasons to exit

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

	// Create our two "demo" spaces.
	struct Space *space = calloc(1, sizeof(struct Space));
	wl_list_insert(&wm.spaces, &space->link);

	space = calloc(1, sizeof(struct Space));
	wl_list_insert(&wm.spaces, &space->link);
}


// Global gunk

static void handle_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
	if (strcmp(interface, river_window_manager_v1_interface.name) == 0) {
		if (version >= 4) {
			window_manager_v1 = wl_registry_bind(registry, name, &river_window_manager_v1_interface, 4);
		}
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

	if (fork() == 0) {
		execlp("swaybg", "swaybg", "-c", "222222", NULL);
	}

	while (true) {
		if (wl_display_dispatch(display) < 0) {
			fprintf(stderr, "dispatch failed\n");
			return 1;
		}
	}
}
