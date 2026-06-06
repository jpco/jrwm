# JrWM - A Junior Window Manager

_(Alternatively, jpco's river window manager)_

JrWM is a tiling window manager written against [the river Wayland
compositor](https://isaacfreund.com/software/river/).

JrWM is a dynamic tiling WM with a layout inspired by that of dwm.  The
implementation is based on that of tinyrwm, although JrWM seems to be,
surprisingly, more fastidiously protocol-rule-following than tinyrwm itself.

JrWM is intended to be low-dependency, easy to build, easy to read and modify,
and to have a good degree of correctness.


## Configuration

Bindings are defined in `bindings.c`, and window management policy is defined in
`manage.c`; that's about as close to configuration as it comes right now.


## Building and installation

To build, JrWM requires:
 - a C99-capable compiler
 - GNU make
 - the wayland-scanner utility (probably from your distribution's `wayland`
   package)
 - libxkbcommon, or your distribution's version

Building is as simple as

```
make
```

Installation is then, as root,

```
make install
```

JrWM will be installed into the /usr/local/bin directory.

The Makefile, like JrWM itself, is intended to be simple and easy to modify.


## TODO

JrWM is still in active development.  Desired improvements and additions
include:

-   Better multi-Space support: more Spaces, better Space/Output assignment,
    dynamic/configurable Spaces
-   Better floating window support, at least to improve the experience with
    dialog windows
-   Behavioral tweaks to more closely match other WMs and expectations
-   (Optional) focus-follows-pointer and pointer-follows-focus behavior, since
    some people like that stuff
-   Actual configuration, probably via IPC or API

In addition, the WM is very untested, and probably somewhat broken, with
multiple outputs.
