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
`river-layer-shell-v1` protocol.

![Screenshot of JrWM](/doc/jrwm.png)

_A screenshot of JrWM featuring waybar, foot, vim,
[es](https://github.com/wryun/es-shell), the
[notcat](https://github.com/jpco/notcat) notification server, and zathura._


## Configuration

Configuration, such as it is, is done by editing the file `config.c`.


## Building and installation

To build, JrWM requires:

 - a C99-capable compiler
 - GNU or BSD make
 - the wayland-scanner utility (probably from your distribution's `wayland`
   package)
 - libxkbcommon, or your distribution's version

Given Wayland and libxkbcommon are already dependencies for river, these are
probably all already installed.

Building should be as simple as

```sh
make
```

To install, run the following as root

```sh
make install
```

JrWM will be installed into the /usr/local/bin directory, and its man page will
be installed into /usr/local/man.

The Makefile, like JrWM itself, is intended to be simple and easy to modify.
