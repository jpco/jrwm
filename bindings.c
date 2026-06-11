// bindings.c -- Key bindings in JrWM and the actions they trigger

// This file is responsible for key bindings and performing actions based on
// those key bindings, including configuring the programs which are spawned.

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

#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include <xkbcommon/xkbcommon.h>

#include "jrwm.h"


// Types

union Arg {
	char **v;
	int32_t i;
};

struct Binddef {
	int32_t mod, key;
	void (*dispatch)(struct Seat *, union Arg);
	union Arg arg;
};

enum BindingManageAction {
	BINDING_MANAGE_NONE,
	BINDING_MANAGE_ENABLE,
	BINDING_MANAGE_DISABLE
};

struct XkbBinding {
	struct wl_list link; // Seat.xkb_bindings
	struct river_xkb_binding_v1 *obj;
	struct Seat *seat;

	void (*dispatch)(struct Seat *, union Arg);
	union Arg arg;
	enum BindingManageAction manage;  // What to do on next window_manage
};


// Binding declarations and configuration

// Bindings to manage single objects
static void binding_spawn(struct Seat *seat, union Arg arg);
static void binding_exit(struct Seat *seat, union Arg arg);
static void binding_close(struct Seat *seat, union Arg arg);
static void binding_toggle_monocle(struct Seat *seat, union Arg arg);

// Bindings to move focus and objects through the WM
static void binding_focus_next(struct Seat *seat, union Arg arg);
static void binding_focus_prev(struct Seat *seat, union Arg arg);
static void binding_move_next(struct Seat *seat, union Arg arg);
static void binding_move_prev(struct Seat *seat, union Arg arg);
static void binding_activate_space(struct Seat *seat, union Arg arg);
static void binding_move_to_space(struct Seat *seat, union Arg arg);


// TODO: De-personalize these so that they are actually useful for other people.
static char *spawn_foot[]	= {"footclient", NULL};
static char *spawn_rofi[]	= {"rofi", "-show", "combi", NULL};
static char *spawn_mute[]	= {"mediactl", "pamixer", "--mute", "--set-volume", "0", NULL};
static char *spawn_volume_up[]	= {"mediactl", "pamixer", "--unmute", "--increase", "5", NULL};
static char *spawn_volume_down[]	= {"mediactl", "pamixer", "--decrease", "5", NULL};
static char *spawn_brightness_up[]	= {"mediactl", "brightnessctl", "-e", "set", "5%+", NULL};
static char *spawn_brightness_down[]	= {"mediactl", "brightnessctl", "-e", "set", "5%-", NULL};


#define super	RIVER_SEAT_V1_MODIFIERS_MOD4
#define shift	RIVER_SEAT_V1_MODIFIERS_SHIFT
#define none	RIVER_SEAT_V1_MODIFIERS_NONE

struct Binddef binds[] = {
	{super, XKB_KEY_q, binding_close, {0}},
	{super|shift, XKB_KEY_e, binding_exit, {0}},
	{super, XKB_KEY_j, binding_focus_next, {0}},
	{super, XKB_KEY_k, binding_focus_prev, {0}},
	{super|shift, XKB_KEY_j, binding_move_next, {0}},
	{super|shift, XKB_KEY_k, binding_move_prev, {0}},
	{super, XKB_KEY_m, binding_toggle_monocle, {0}},

	{super, XKB_KEY_1, binding_activate_space, {.i = 1}},
	{super, XKB_KEY_2, binding_activate_space, {.i = 2}},
	{super, XKB_KEY_3, binding_activate_space, {.i = 3}},
	{super, XKB_KEY_4, binding_activate_space, {.i = 4}},
	{super, XKB_KEY_5, binding_activate_space, {.i = 5}},
	{super, XKB_KEY_6, binding_activate_space, {.i = 6}},
	{super, XKB_KEY_7, binding_activate_space, {.i = 7}},
	{super, XKB_KEY_8, binding_activate_space, {.i = 8}},
	{super, XKB_KEY_9, binding_activate_space, {.i = 9}},

	{super|shift, XKB_KEY_1, binding_move_to_space, {.i = 1}},
	{super|shift, XKB_KEY_2, binding_move_to_space, {.i = 2}},
	{super|shift, XKB_KEY_3, binding_move_to_space, {.i = 3}},
	{super|shift, XKB_KEY_4, binding_move_to_space, {.i = 4}},
	{super|shift, XKB_KEY_5, binding_move_to_space, {.i = 5}},
	{super|shift, XKB_KEY_6, binding_move_to_space, {.i = 6}},
	{super|shift, XKB_KEY_7, binding_move_to_space, {.i = 7}},
	{super|shift, XKB_KEY_8, binding_move_to_space, {.i = 8}},
	{super|shift, XKB_KEY_9, binding_move_to_space, {.i = 9}},

	{super, XKB_KEY_Return, binding_spawn, {.v = spawn_foot}},
	{super, XKB_KEY_space, binding_spawn, {.v = spawn_rofi}},
	{none, XKB_KEY_XF86AudioMute, binding_spawn, {.v = spawn_mute}},
	{none, XKB_KEY_XF86AudioRaiseVolume, binding_spawn, {.v = spawn_volume_up}},
	{none, XKB_KEY_XF86AudioLowerVolume, binding_spawn, {.v = spawn_volume_down}},
	{none, XKB_KEY_XF86MonBrightnessUp, binding_spawn, {.v = spawn_brightness_up}},
	{none, XKB_KEY_XF86MonBrightnessDown, binding_spawn, {.v = spawn_brightness_down}},
	{0, 0, NULL, {0}}
};

#undef super
#undef shift
#undef none


// Utility functions for bindings

static struct Window *next_window(struct Window *window, struct wl_list *list) {
	if (window == NULL)
		return NULL;
	if (list == NULL)
		list = &window->link;

	struct Window *w;
	wl_list_for_each(w, list, link)
		if (w->space == window->space)
			return w;
	return NULL;
}

static struct Window *prev_window(struct Window *window, struct wl_list *list) {
	if (window == NULL)
		return NULL;
	if (list == NULL)
		list = &window->link;

	struct Window *w;
	wl_list_for_each_reverse(w, list, link)
		if (w->space == window->space)
			return w;
	return NULL;
}

static struct Space *nth_space(int n) {
	int i = 0;
	struct Space *space;
	wl_list_for_each(space, &wm.spaces, link)
		if ((++i) == n)
			return space;
	return NULL;
}


// Binding function definitions
// None of these functions run during a manage or render sequence

static void binding_spawn(struct Seat *seat, union Arg arg) {
	struct sigaction sa;
	if (fork() == 0) {
		setsid();

		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sa.sa_handler = SIG_DFL;
		sigaction(SIGCHLD, &sa, NULL);

		execvp((const char *)arg.v[0], (char *const *)arg.v);
		exit(12);  // Just in case
	}
}

static void binding_exit(struct Seat *seat, union Arg arg) {
	river_window_manager_v1_exit_session(window_manager_v1);
}

static void binding_close(struct Seat *seat, union Arg arg) {
	if (seat->focused->focused != NULL)
		seat->focused->focused->close = true;
}

static void binding_toggle_monocle(struct Seat *seat, union Arg arg) {
	struct Space *space = seat->focused;
	if (space->layout == tiled_layout)
		space->layout = monocle_layout;
	else
		space->layout = tiled_layout;
}

static void binding_focus_next(struct Seat *seat, union Arg arg) {
	struct Window *window = next_window(seat->focused->focused, NULL);
	if (window == NULL)
		return;
	seat->focused->focused = window;
	seat->warp = true;
}

static void binding_focus_prev(struct Seat *seat, union Arg arg) {
	struct Window *window = prev_window(seat->focused->focused, NULL);
	if (window == NULL)
		return;
	seat->focused->focused = window;
	seat->warp = true;
}

static void binding_move_next(struct Seat *seat, union Arg arg) {
	struct Window *window = seat->focused->focused;
	struct Window *target = next_window(window, NULL);
	if (window == NULL || target == NULL)
		return;

	bool to_first = (target == next_window(window, &wm.windows));
	wl_list_remove(&window->link);
	if (to_first)
		wl_list_insert(&wm.windows, &window->link);
	else
		wl_list_insert(&target->link, &window->link);
	seat->warp = true;
}

static void binding_move_prev(struct Seat *seat, union Arg arg) {
	struct Window *window = seat->focused->focused;
	struct Window *target = prev_window(window, NULL);
	if (window == NULL || target == NULL)
		return;

	bool to_last = (target == prev_window(window, &wm.windows));
	wl_list_remove(&window->link);
	if (to_last)
		wl_list_insert(wm.windows.prev, &window->link);
	else
		wl_list_insert(target->link.prev, &window->link);
	seat->warp = true;
}

// Activate and focus the nth Space
static void binding_activate_space(struct Seat *seat, union Arg arg) {
	struct Space *space = nth_space(arg.i);
	if (space == NULL)
		return;

	// If the Space is "idle", yank it here
	if (space->output == NULL || idle_space(space))
		space->output = seat->focused->output;
	space->output->active = space;
	seat->focused = space;
}

// Move the currently focused Window to the nth Space
static void binding_move_to_space(struct Seat *seat, union Arg arg) {
	struct Window *window = seat->focused->focused;
	struct Space *space = nth_space(arg.i);
	if (window == NULL || space == NULL)
		return;

	replace_window(window);  // Sensible?
	window->space = space;
	space->focused = window;
}


// "Core" functions defining and dispatching bindings

static void xkb_binding_handle_pressed(void *data, struct river_xkb_binding_v1 *obj) {
	struct XkbBinding *binding = data;
	binding->dispatch(binding->seat, binding->arg);
}

static void xkb_binding_handle_released(void *data, struct river_xkb_binding_v1 *obj) {}

const struct river_xkb_binding_v1_listener river_xkb_binding_listener = {
	.pressed = xkb_binding_handle_pressed,
	.released = xkb_binding_handle_released,
};

static void xkb_binding_create(struct Seat *seat, uint32_t mods, xkb_keysym_t keysym, void (*dispatch)(struct Seat *seat, union Arg arg), union Arg arg) {
	struct XkbBinding *binding = calloc(1, sizeof(struct XkbBinding));
	binding->obj = river_xkb_bindings_v1_get_xkb_binding(xkb_bindings_v1, seat->obj, keysym, mods);
	binding->seat = seat;
	binding->dispatch = dispatch;
	binding->arg = arg;
	binding->manage = BINDING_MANAGE_ENABLE;

	river_xkb_binding_v1_add_listener(binding->obj, &river_xkb_binding_listener, binding);
	wl_list_insert(&seat->xkb_bindings, &binding->link);
}

extern void init_xkb_bindings(struct Seat *seat) {
	int i;
	for (i = 0; binds[i].dispatch != NULL; i++)
		xkb_binding_create(seat, binds[i].mod, binds[i].key,
				binds[i].dispatch, binds[i].arg);
}

extern void manage_xkb_bindings(struct Seat *seat) {
	struct XkbBinding *binding;
	wl_list_for_each(binding, &seat->xkb_bindings, link) {
		switch (binding->manage) {
		case BINDING_MANAGE_ENABLE:
			river_xkb_binding_v1_enable(binding->obj);
			break;
		case BINDING_MANAGE_DISABLE:
			river_xkb_binding_v1_disable(binding->obj);
			break;
		case BINDING_MANAGE_NONE:
			break;
		}
		binding->manage = BINDING_MANAGE_NONE;
	}
}

extern void remove_xkb_bindings(struct Seat *seat) {
	struct XkbBinding *binding, *tmp;
	wl_list_for_each_safe(binding, tmp, &seat->xkb_bindings, link) {
		river_xkb_binding_v1_destroy(binding->obj);
		wl_list_remove(&binding->link);
		free(binding);
	}
}

extern void lock_xkb_bindings(struct Seat *seat) {
	struct XkbBinding *binding;
	wl_list_for_each(binding, &seat->xkb_bindings, link)
		binding->manage = BINDING_MANAGE_DISABLE;
}

extern void unlock_xkb_bindings(struct Seat *seat) {
	struct XkbBinding *binding;
	wl_list_for_each(binding, &seat->xkb_bindings, link)
		binding->manage = BINDING_MANAGE_ENABLE;
}
