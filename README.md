# JrWM - A Junior Window Manager

_(Alternatively, jpco's river window manager)_

JrWM is a dynamic tiling window manager written for [the river Wayland
compositor](https://isaacfreund.com/software/river/).
It is designed to be small, low-dependency, easy to build, read and modify, and
to have a good degree of correctness.

Windows in JrWM are organized into spaces, which are collections of windows
associated with an output and a layout.  Multiple spaces may be associated with
an output simultaneously, but only one space is visible on an output at any one
time.  Supported layouts are a tiled layout like that of dwm and a monocle
layout.

JrWM supports very little in the way of visuals.  Window borders are drawn to
indicate focus, but for anything else, additional programs such as waybar must
be used.  For the sake of these programs, JrWM supports the
river-layer-shell-v1 protocol.

![Screenshot of JrWM](/doc/jrwm.png)

_A screenshot of JrWM featuring waybar, foot, vim,
[es](https://github.com/wryun/es-shell), the [notcat](https://github.com/jpco/notcat) notification server, and zathura._


## Configuration

Configuration, such as it is, is done by editing the source:

-   `bindings.c` contains key bindings
-   `layout.c` contains window decoration and layout
-   `jrwm.c` interacts with Wayland


## Building and installation

To build, JrWM requires:

 - a C99-capable compiler
 - GNU or BSD make
 - the wayland-scanner utility (probably from your distribution's `wayland`
   package)
 - libxkbcommon, or your distribution's version

Given Wayland and libxkbcommon are already dependencies for river, these are
probably all already installed.

Building is as simple as

```
make
```

Installation is then, as root,

```
make install
```

JrWM will be installed into the /usr/local/bin directory, and its man page will
be installed into /usr/local/man.

The Makefile, like JrWM itself, is intended to be simple and easy to modify.


## Code style guidelines

These reminders are listed here primarily to remind myself to avoid my own bad
habits:

-   Functions should either be "real", pure functions, or should be
    state-modifying procedures; don't return a value from a function that
    modifies state or makes calls to River/Wayland.
-   Loops and conditionals should always have braces, unless they contain only
    other conditionals without braces or a single statement with no comment (and
    even then, braces are okay).  An if statement should have braces on all its
    "then" statements or on none of them.
-   Files should be ordered: types, variables, static functions, and finally
    extern functions (or main).  If this order becomes too obnoxious, that's
    evidence the file should be split up.


## TODO

JrWM is still in active development.  Desired improvements and additions
include:

-   Floating window support, and "dialog"/child windows as floating windows
-   Better ability to add/remove spaces
-   Multi-seat support
-   Fix up bad `wl_list` handling

Some possible further additions:

-   Better flexibility for layouts - e.g., binary space partitioning, tags,
    scrolling
-   Optional focus-follows-pointer and pointer-follows-focus behavior, since
    some people like that stuff
-   Optional extensions for built-in input and output management, presuming they
    can do it better than external tools (e.g., frame-perfect output scaling)
-   Actual configuration, probably via IPC or API, once the codebase has settled
    down
