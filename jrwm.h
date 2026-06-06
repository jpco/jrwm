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

	struct Rect windowed;  // Non-exclusive area of the Output

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


// manage.c

extern void seat_do_focus(struct Seat *);
extern void window_do_deferred(struct Window *);

extern void manage_output(struct Output *);
extern void render_output(struct Output *);


// bindings.c

extern void init_xkb_bindings(struct Seat *);
extern void enable_xkb_bindings(struct Seat *);
extern void remove_xkb_bindings(struct Seat *);

#endif
