# JrWM - A Junior Window Manager

_(Alternatively, jpco's river window manager)_

JrWM is a tiling window manager written against [the river Wayland
compositor](https://isaacfreund.com/software/river/).

JrWM is a dynamic tiling WM with a layout inspired by that of dwm.  The
implementation is based on that of tinyrwm, although JrWM seems to be,
surprisingly, more fastidiously protocol-rule-following than tinyrwm itself.

JrWM is intended to be low-dependency, both to build and to run.


## Configuration

Bindings are defined in `bindings.c`, and window management policy is defined in
`manage.c`; that's about as close to configuration as it comes right now.


## Building and installation

To build, JrWM requires:
 - a C99-capable compiler
 - GNU make (easy to make portable with small changes to the makefile)
 - the wayland-scanner utility (probably from your distribution's `wayland`
   package)
 - libxkbcommon, or your distribution's version

Building is as simple as `make`.  Installation is then `make install` as root.
JrWM will be installed by default into the /usr/local/bin directory.

To run, JrWM requires river, of course.


## TODO

JrWM is still in active development.  Desired improvements and additions
include:

-   Better multi-Space support: more Spaces, better Space/Output assignment,
    dynamic/configurable Spaces
-   Better floating window support, at least to improve the experience with
    child windows
-   Behavioral tweaks to more closely match other WMs and expectations around
    window movement, focus, output management, etc.
-   (Optional) focus-follows-pointer and pointer-follows-focus behavior
-   Actual configuration, probably via IPC or API
-   Additional minor, nice behaviors: output scaling, libinput config (right
    clicks!), background color

In addition, the WM is very untested, and probably somewhat broken, with
multiple outputs.
