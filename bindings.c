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


// Utility functions for bindings

static void activate_space(struct Seat *seat, struct Space *space) {
	if (space == NULL)
		return;

	// If the Space isn't already active on any Output, yank it here
	if (active_on_output(space) == NULL)
		space->output = seat->focused->output;

	if (space->output->active != space) {
		struct Space *tmp = space->output->active;
		space->output->active = space;
		collect_space(tmp);
	}
	seat->focused = space;
}

static void move_to_space(struct Window *window, struct Space *space) {
	if (window == NULL || space == NULL)
		return;

	replace_window(window);  // Sensible?
	window->space = space;
	space->focused = window;
}

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

static struct Space *next_space(struct Space *space, bool (*filter)(struct Space *)) {
	struct Space *target;
	wl_list_for_each(target, &space->link, link) {
		if (&target->link == &wm.spaces)
			continue;
		if (filter(target))
			return target;
	}
	return NULL;
}

static struct Space *prev_space(struct Space *space, bool (*filter)(struct Space *)) {
	struct Space *target;
	wl_list_for_each_reverse(target, &space->link, link) {
		if (&target->link == &wm.spaces)
			continue;
		if (filter(target))
			return target;
	}
	return NULL;
}


// Binding function definitions
// None of these functions run during a manage or render sequence

extern void binding_spawn(struct Seat *seat, union Arg arg) {
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

extern void binding_exit(struct Seat *seat, union Arg arg) {
	river_window_manager_v1_exit_session(window_manager_v1);
}

extern void binding_close(struct Seat *seat, union Arg arg) {
	if (seat->focused->focused != NULL)
		seat->focused->focused->close = true;
}

extern void binding_toggle_fake_fullscreen(struct Seat *seat, union Arg arg) {
	struct Window *window = seat->focused->focused;
	if (window == NULL)
		return;
	if (window->fake_fullscreen)
		window->exit_fullscreen = true;
	else
		window->enter_fake_fullscreen = true;
}

extern void binding_toggle_fullscreen(struct Seat *seat, union Arg arg) {
	struct Window *window = seat->focused->focused;
	if (window == NULL)
	return;
	if (window->fullscreen)
		window->exit_fullscreen = true;
	else
		window->enter_fullscreen = true;
}

extern void binding_toggle_monocle(struct Seat *seat, union Arg arg) {
	struct Space *space = seat->focused;
	if (space->layout != monocle_layout)
		space->layout = monocle_layout;
	else
		space->layout = default_layout;
}

extern void binding_focus_next(struct Seat *seat, union Arg arg) {
	struct Window *window = next_window(seat->focused->focused, NULL);
	if (window == NULL)
		return;
	seat->focused->focused = window;
	seat->warp = true;
}

extern void binding_focus_prev(struct Seat *seat, union Arg arg) {
	struct Window *window = prev_window(seat->focused->focused, NULL);
	if (window == NULL)
		return;
	seat->focused->focused = window;
	seat->warp = true;
}

extern void binding_move_next(struct Seat *seat, union Arg arg) {
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

extern void binding_move_prev(struct Seat *seat, union Arg arg) {
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

extern void binding_activate_space(struct Seat *seat, union Arg arg) {
	activate_space(seat, nth_space(arg.i));
}

extern void binding_activate_next_busy_space(struct Seat *seat, union Arg arg) {
	activate_space(seat, next_space(seat->focused, busy_space));
}

extern void binding_activate_next_idle_space(struct Seat *seat, union Arg arg) {
	activate_space(seat, next_space(seat->focused, idle_space));
}

extern void binding_activate_next_space(struct Seat *seat, union Arg arg) {
	activate_space(seat, next_space(seat->focused, any_space));
}

extern void binding_activate_prev_busy_space(struct Seat *seat, union Arg arg) {
	activate_space(seat, prev_space(seat->focused, busy_space));
}

extern void binding_activate_prev_idle_space(struct Seat *seat, union Arg arg) {
	activate_space(seat, prev_space(seat->focused, idle_space));
}

extern void binding_activate_prev_space(struct Seat *seat, union Arg arg) {
	activate_space(seat, prev_space(seat->focused, any_space));
}

// Move the currently focused Window to the nth Space
extern void binding_move_to_space(struct Seat *seat, union Arg arg) {
	move_to_space(seat->focused->focused, nth_space(arg.i));
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
