// bindings.c -- Key bindings in JrWM and the actions they trigger
//
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
#include <xkbcommon/xkbcommon-keysyms.h>

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

struct XkbBinding {
	struct wl_list link; // Seat.xkb_bindings
	struct river_xkb_binding_v1 *obj;
	struct Seat *seat;

	void (*dispatch)(struct Seat *, union Arg);
	union Arg arg;
	bool enable;  // Enable this binding on the next window_manage
};


// Binding declarations and configuration

static void binding_spawn(struct Seat *seat, union Arg arg);
static void binding_exit(struct Seat *seat, union Arg arg);
static void binding_close(struct Seat *seat, union Arg arg);
static void binding_focus_next(struct Seat *seat, union Arg arg);
static void binding_focus_prev(struct Seat *seat, union Arg arg);
static void binding_move_next(struct Seat *seat, union Arg arg);
static void binding_move_prev(struct Seat *seat, union Arg arg);
static void binding_toggle_maximize(struct Seat *seat, union Arg arg);
static void binding_switch_space(struct Seat *seat, union Arg arg);


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
	{super, XKB_KEY_q, binding_close, {}},
	{super|shift, XKB_KEY_e, binding_exit, {}},
	{super, XKB_KEY_j, binding_focus_next, {}},
	{super, XKB_KEY_k, binding_focus_prev, {}},
	{super|shift, XKB_KEY_j, binding_move_next, {}},
	{super|shift, XKB_KEY_k, binding_move_prev, {}},
	{super, XKB_KEY_m, binding_toggle_maximize, {}},
	{super, XKB_KEY_s, binding_switch_space, {}},

	{super, XKB_KEY_Return, binding_spawn, {.v = spawn_foot}},
	{super, XKB_KEY_space, binding_spawn, {.v = spawn_rofi}},
	{none, XKB_KEY_XF86AudioMute, binding_spawn, {.v = spawn_mute}},
	{none, XKB_KEY_XF86AudioRaiseVolume, binding_spawn, {.v = spawn_volume_up}},
	{none, XKB_KEY_XF86AudioLowerVolume, binding_spawn, {.v = spawn_volume_down}},
	{none, XKB_KEY_XF86MonBrightnessUp, binding_spawn, {.v = spawn_brightness_up}},
	{none, XKB_KEY_XF86MonBrightnessDown, binding_spawn, {.v = spawn_brightness_down}},
	{0, 0, NULL, {}}
};

#undef super
#undef shift
#undef none


// Binding function definitions

static void binding_spawn(struct Seat *seat, union Arg arg) {
	if (fork() == 0) {
		setsid();
		signal(SIGCHLD, SIG_DFL);
		execvp((const char*)arg.v[0], (char *const *)arg.v);
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

static void binding_focus_next(struct Seat *seat, union Arg arg) {
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

static void binding_focus_prev(struct Seat *seat, union Arg arg) {
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

static void binding_move_next(struct Seat *seat, union Arg arg) {
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

static void binding_move_prev(struct Seat *seat, union Arg arg) {
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

static void binding_toggle_maximize(struct Seat *seat, union Arg arg) {
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

static void binding_switch_space(struct Seat *seat, union Arg arg) {
	struct Space *space = seat->focused, *s, *unfocused = NULL;
	wl_list_for_each(s, &wm.spaces, link) {
		if (s != space)
			unfocused = s;
	}
	unfocused->output = seat->focused->output;
	unfocused->output->active = unfocused;
	seat->focused = unfocused;
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
	binding->enable = true;

	river_xkb_binding_v1_add_listener(binding->obj, &river_xkb_binding_listener, binding);
	wl_list_insert(&seat->xkb_bindings, &binding->link);
}

extern void init_xkb_bindings(struct Seat *seat) {
	int i;
	for (i = 0; binds[i].dispatch != NULL; i++) {
		xkb_binding_create(seat, binds[i].mod, binds[i].key, binds[i].dispatch, binds[i].arg);
	}
}

extern void enable_xkb_bindings(struct Seat *seat) {
	struct XkbBinding *binding;
	wl_list_for_each(binding, &seat->xkb_bindings, link) {
		if (binding->enable) {
			river_xkb_binding_v1_enable(binding->obj);
			binding->enable = false;
		}
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
