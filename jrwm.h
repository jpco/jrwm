// jrwm.h -- Header file for JrWM.

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

#ifndef JRWM_H
#define JRWM_H

#include <stdbool.h>

#include <river-layer-shell-v1.h>
#include <river-window-management-v1.h>
#include <river-xkb-bindings-v1.h>


// Types

struct Rect {
	int32_t x, y, width, height;
};

// A Space represents a collection of Windows on an Output with a layout.
// Space is the "clearing-house" type for the WM; there MUST be at least one
// Space for everything else to point at, and there SHOULD be at least one Space
// per Output.
struct Space {
	struct wl_list link;    // WindowManager.spaces
	struct Output *output;  // May be null
	struct Window *focused; // May be null

	bool is_static;         // Static spaces are never removed

	void (*layout)(struct Space *space, struct Rect bounds);
};

// An Output is like an actual physical display.
struct Output {
	struct wl_list link;    // WindowManager.outputs
	struct river_output_v1 *obj;
	struct river_layer_shell_output_v1 *ls;

	struct Rect real, windowed;

	struct Space *active;   // Non-null
};

// A Window is a rectangle under management.
struct Window {
	struct wl_list link;    // WindowManager.windows
	struct river_window_v1 *obj;
	struct river_node_v1 *node;

	bool maximized;  // The window has been inform_maximized
	bool fullscreen; // The window is fullscreen
	bool fake_fullscreen; // The window acts as if fullscreen

	// Deferred tasks for the manage sequence
	bool set_capabilities;  // window_v1.set_capabilities
	bool close;             // window_v1.close
	bool enter_fullscreen;  // window_v1.inform_fullscreen
	bool exit_fullscreen;   // window_v1.inform_not_fullscreen
	bool enter_fake_fullscreen;
	bool exit_fake_fullscreen;

	// Information for the render sequence
	struct Rect layout;

	struct Space *space;    // Non-null
};

// A Seat is a collection of input devices.
struct Seat {
	struct wl_list link;    // WindowManager.seats
	struct river_seat_v1 *obj;
	struct river_layer_shell_seat_v1 *ls;
	struct wl_list xkb_bindings;  // XkbBinding

	bool warp;              // Warp pointer to focused window
	bool moved;             // The pointer has moved since last manage
	struct Window *entered; // This window has been entered

	bool ls_focused;        // Layer shell surface has focus
	struct Space *focused;  // Non-null
};

// Args and Binddefs are used to configure XkbBindings
union Arg {
	char **v;
	int32_t i;
};

struct Binddef {
	int32_t mod, key;
	void (*dispatch)(struct Seat *, union Arg);
	union Arg arg;
};

struct WindowManager {
	struct wl_list outputs; // Output
	struct wl_list windows; // Window
	struct wl_list seats;   // Seat
	struct wl_list spaces;  // Space
};


// jrwm.c

extern struct WindowManager wm;

extern struct river_window_manager_v1 *window_manager_v1;
extern struct river_xkb_bindings_v1 *xkb_bindings_v1;
extern struct river_layer_shell_v1 *layer_shell_v1;

extern bool idle_space(struct Space *);
extern struct Output *active_on_output(struct Space *);
extern struct Space *create_space(void);
extern void collect_space(struct Space *);


// layout.c

// Layout functions
extern void tiled_layout(struct Space *, struct Rect);
extern void monocle_layout(struct Space *, struct Rect);

// Called on creation of objects to manage internal pointers
extern void place_output(struct Output *);
extern void place_window(struct Window *);
extern void place_seat(struct Seat *);

// Called on deletion of objects
extern void replace_output(struct Output *);
extern void replace_window(struct Window *);

// Called during the manage sequence
extern void manage_window_deferred(struct Window *);
extern void manage_space(struct Space *);
extern void manage_seat_focus(struct Seat *);

// Called during the render sequence
extern void render_space(struct Space *);
extern void render_seat_focus(struct Seat *);


// bindings.c

// Binding functions which may be called from Binddefs
extern void binding_spawn(struct Seat *, union Arg);
extern void binding_exit(struct Seat *, union Arg);
extern void binding_close(struct Seat *, union Arg);

extern void binding_toggle_fake_fullscreen(struct Seat *, union Arg);
extern void binding_toggle_fullscreen(struct Seat *, union Arg);
extern void binding_toggle_monocle(struct Seat *, union Arg);

extern void binding_focus_next(struct Seat *, union Arg);
extern void binding_focus_prev(struct Seat *, union Arg);
extern void binding_move_next(struct Seat *, union Arg);
extern void binding_move_prev(struct Seat *, union Arg);

extern void binding_activate_next_busy_space(struct Seat *, union Arg);
extern void binding_activate_prev_busy_space(struct Seat *, union Arg);
extern void binding_activate_next_idle_space(struct Seat *, union Arg);
extern void binding_activate_prev_idle_space(struct Seat *, union Arg);
extern void binding_activate_next_space(struct Seat *, union Arg);
extern void binding_activate_prev_space(struct Seat *, union Arg);

extern void binding_activate_space(struct Seat *, union Arg);
extern void binding_move_to_space(struct Seat *, union Arg);

// Functions used to manage bindings
extern void init_xkb_bindings(struct Seat *);
extern void manage_xkb_bindings(struct Seat *);
extern void remove_xkb_bindings(struct Seat *);
extern void lock_xkb_bindings(struct Seat *);
extern void unlock_xkb_bindings(struct Seat *);


// config.c

extern int static_spaces;
extern void (*default_layout)(struct Space *, struct Rect);

extern uint32_t border_color[4];
extern uint32_t focused_color[4];

extern int monocle_borderpx;

extern int tiled_borderpx;
extern int tiled_margin;
extern int tiled_output_padding;
extern float tiled_splitratio;

extern bool focus_follows_pointer;
extern bool pointer_follows_focus;

extern struct Binddef binds[];


#endif
