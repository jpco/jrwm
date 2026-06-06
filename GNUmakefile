# Basic make variables.

CC	= gcc
INSTALL	= /usr/bin/install -c
MKDIR_P	= /usr/bin/mkdir -p

INSTALL_DIR	= /usr/local/bin

CFLAGS	= -std=c99 -g -O2 -Wall -I$(PROTO)
LDFLAGS	= -lwayland-client -lxkbcommon

HFILES	= jrwm.h
CFILES	= jrwm.c manage.c bindings.c
OFILES	= jrwm.o manage.o bindings.o


# Generated file variables. Could be cleaner.

PROTO	= ./protocol

PROTOS	= $(PROTO)/river-layer-shell-v1.xml $(PROTO)/river-window-management-v1.xml $(PROTO)/river-xkb-bindings-v1.xml
PROTOC	= $(PROTOS:.xml=.c)
PROTOH	= $(PROTOS:.xml=.h)
PROTOO	= $(PROTOS:.xml=.o)


# "Manual" targets.

jrwm	: $(OFILES) $(PROTOO) $(PROTOC)
	$(CC) -o jrwm $(LDFLAGS) -o jrwm $(OFILES) $(PROTOO)

clean	:
	rm -f jrwm $(OFILES) $(PROTOO) $(PROTOC) $(PROTOH)

install	: jrwm
	$(MKDIR_P) $(INSTALL_DIR)
	$(INSTALL) -s jrwm $(INSTALL_DIR)


# Dependencies (could be generated).

jrwm.o		: jrwm.c jrwm.h $(PROTOH)
bindings.o	: bindings.c jrwm.h $(PROTOH)
manage.o	: manage.c jrwm.h $(PROTOH)


# Generated file recipes.  This is the GNU extensions; should be made portable.

$(PROTO)/%.o	: $(PROTO)/%.c $(PROTO)/%.h

$(PROTO)/%.c	: $(PROTO)/%.xml
	wayland-scanner private-code $< $@

$(PROTO)/%.h	: $(PROTO)/%.xml
	wayland-scanner client-header $< $@
