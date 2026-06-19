// config.c -- Static configuration for JrWM

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

#include <xkbcommon/xkbcommon.h>

#include "jrwm.h"

// Space management

// How many static Spaces are allocated on startup.  More spaces than this may
// be created dynamically as needed, but there will always be at least this many
int static_spaces = 9;

// The default layout for a newly-created Space
void (*default_layout)(struct Space *, struct Rect) = tiled_layout;


// "Aesthetics", insofar as they exist: Colors and borders

#define	COLOR(hex)	{ ((hex >> 24) & 0xFF) * (UINT32_MAX / 255), \
			  ((hex >> 16) & 0xFF) * (UINT32_MAX / 255), \
			  ((hex >>  8) & 0xFF) * (UINT32_MAX / 255), \
			  ( hex        & 0xFF) * (UINT32_MAX / 255) }

uint32_t border_color[4]  = COLOR(0x333333ff);
uint32_t focused_color[4] = COLOR(0x77aa99ff);

int monocle_borderpx = 0;
int tiled_borderpx   = 2;


// Tiled layout config

int tiled_margin         = -2;	// Space between windows
int tiled_output_padding =  0;	// Space around windows
float tiled_splitratio   =  0.52;


// Pointer behavior

bool focus_follows_pointer = true;
bool pointer_follows_focus = false;


// Keybinds and spawns

static char *spawn_foot[] = {"footclient", NULL};
static char *spawn_rofi[] = {"rofi", "-show", "combi", NULL};
static char *spawn_mute[] = {"mediactl", "pamixer", "--mute", "--set-volume", "0", NULL};
static char *spawn_volume_up[] = {"mediactl", "pamixer", "--unmute", "--increase", "5", NULL};
static char *spawn_volume_down[] = {"mediactl", "pamixer", "--decrease", "5", NULL};
static char *spawn_brightness_up[] = {"mediactl", "brightnessctl", "-e", "set", "5%+", NULL};
static char *spawn_brightness_down[] = {"mediactl", "brightnessctl", "-e", "set", "5%-", NULL};


#define super	RIVER_SEAT_V1_MODIFIERS_MOD4
#define shift	RIVER_SEAT_V1_MODIFIERS_SHIFT
#define none	RIVER_SEAT_V1_MODIFIERS_NONE

// The key codes of the form XKB_KEY_* are declared in xkbcommon.h
// The binding functions are declared in jrwm.h and defined in bindings.c
struct Binddef binds[] = {
	{super,       XKB_KEY_q, binding_close,          {0}},
	{super|shift, XKB_KEY_e, binding_exit,           {0}},
	{super,       XKB_KEY_j, binding_focus_next,     {0}},
	{super|shift, XKB_KEY_j, binding_move_next,      {0}},
	{super,       XKB_KEY_k, binding_focus_prev,     {0}},
	{super|shift, XKB_KEY_k, binding_move_prev,      {0}},
	{super,       XKB_KEY_m, binding_toggle_monocle, {0}},
	{super|shift, XKB_KEY_m, binding_toggle_fake_fullscreen, {0}},
	{super|shift, XKB_KEY_f, binding_toggle_fullscreen, {0}},

	// Bindings to refer to spaces by number; best used with static_spaces = 9
	{super,       XKB_KEY_1, binding_activate_space, {.i = 1}},
	{super|shift, XKB_KEY_1, binding_move_to_space,  {.i = 1}},
	{super,       XKB_KEY_2, binding_activate_space, {.i = 2}},
	{super|shift, XKB_KEY_2, binding_move_to_space,  {.i = 2}},
	{super,       XKB_KEY_3, binding_activate_space, {.i = 3}},
	{super|shift, XKB_KEY_3, binding_move_to_space,  {.i = 3}},
	{super,       XKB_KEY_4, binding_activate_space, {.i = 4}},
	{super|shift, XKB_KEY_4, binding_move_to_space,  {.i = 4}},
	{super,       XKB_KEY_5, binding_activate_space, {.i = 5}},
	{super|shift, XKB_KEY_5, binding_move_to_space,  {.i = 5}},
	{super,       XKB_KEY_6, binding_activate_space, {.i = 6}},
	{super|shift, XKB_KEY_6, binding_move_to_space,  {.i = 6}},
	{super,       XKB_KEY_7, binding_activate_space, {.i = 7}},
	{super|shift, XKB_KEY_7, binding_move_to_space,  {.i = 7}},
	{super,       XKB_KEY_8, binding_activate_space, {.i = 8}},
	{super|shift, XKB_KEY_8, binding_move_to_space,  {.i = 8}},
	{super,       XKB_KEY_9, binding_activate_space, {.i = 9}},
	{super|shift, XKB_KEY_9, binding_move_to_space,  {.i = 9}},

#define spawn_binding(mod, key, argv)	{mod, key, binding_spawn, {.v = argv}}

	spawn_binding(super, XKB_KEY_Return, spawn_foot),
	spawn_binding(super, XKB_KEY_space,  spawn_rofi),

	spawn_binding(none,  XKB_KEY_XF86AudioMute,         spawn_mute),
	spawn_binding(none,  XKB_KEY_XF86AudioRaiseVolume,  spawn_volume_up),
	spawn_binding(none,  XKB_KEY_XF86AudioLowerVolume,  spawn_volume_down),
	spawn_binding(none,  XKB_KEY_XF86MonBrightnessUp,   spawn_brightness_up),
	spawn_binding(none,  XKB_KEY_XF86MonBrightnessDown, spawn_brightness_down),

#undef spawn_binding

	{0, 0, NULL, {0}}
};

#undef super
#undef shift
#undef none
