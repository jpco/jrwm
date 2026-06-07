# JrWM - A Junior Window Manager

_(Alternatively, jpco's river window manager)_

JrWM is a tiling window manager written for [the river Wayland
compositor](https://isaacfreund.com/software/river/).

JrWM is a dynamic tiling WM with a layout inspired by that of dwm.  The
implementation is based on that of tinyrwm, although JrWM seems to be,
surprisingly, more fastidiously protocol-rule-following than tinyrwm itself.

JrWM supports:
-   Multiple "spaces" (workspaces) per output
-   Layer shell exclusive and non-exclusive surfaces
-   Window maximization and fullscreen
-   Per-space layouts, either tiled (`dwm` style) or monocle
-   Reaping child processes, giving them correct signal handlers, and placing
    them in their own session

JrWM is intended to be low-dependency, easy to build, easy to read and modify,
and to have a good degree of correctness.

![Screenshot of JrWM](/doc/jrwm.png)

_A screenshot of JrWM featuring waybar, foot, vim,
[notcat](https://github.com/jpco/notcat),
[es](https://github.com/wryun/es-shell) and zathura._


## Configuration

Configuration, such as it is, is done by editing the source:

-   `bindings.c` contains key bindings
-   `layout.c` contains window decoration and layout
-   `jrwm.c` contains argv of optional command to run on startup


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


## Code style guidelines

These reminders are listed here primarily to remind myself to avoid my own bad
habits:

-   Functions should either be "real" pure functions, or should be
    state-modifying procedures; don't return a value from a function that
    modifies state or makes calls to River/Wayland.
-   Loops and conditionals should always have braces, unless they contain only
    other conditionals without braces or a single statement with no comment (and
    even then, braces are okay).  An if statement should have braces on all its
    "then" statements or on none of them.
-   Files should be ordered: types, variables, static functions, and finally
    extern functions (or main).


## TODO

JrWM is still in active development.  Desired improvements and additions
include:

-   Better multi-space support: more spaces, better space/output assignment,
    dynamic/configurable spaces
-   Improve behavior around adding and removing outputs and windows (who gets
    focus?  Where do spaces get assigned?  How do we handle not enough, or too
    many, spaces?)
-   Floating window support, and "dialog"/child windows as floating windows
-   Documentation, probably via man page
-   JrWM is untested, and so probably buggy, with multiple outputs or seats; we
    should fix that

Some possible further additions:

-   Consider whether a tag-oriented setup can/should be supported as an
    alternative to a space-oriented setup (spaces will still be important
    internal data structures, just not necessarily user-affecting)
-   Optional focus-follows-pointer and pointer-follows-focus behavior, since
    some people like that stuff
-   Optional extensions for input and output management, presuming they enable
    better "frame perfection" and other behaviors than external tools do
-   Actual configuration, probably via IPC or API, once the codebase has settled
    down
