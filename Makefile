# Basic make variables.

CC	= gcc
INSTALL	= /usr/bin/install -c
MKDIR_P	= /usr/bin/mkdir -p

INSTALL_DIR	= /usr/local/bin
PROTO_DIR	= ./protocol

CFLAGS	= -g -O2 -Wall -I$(PROTO_DIR) -lwayland-client -lxkbcommon

CFILES	= jrwm.c manage.c bindings.c $(PROTOC)
HFILES	= jrwm.h $(PROTOH)


# Generated file variables.

PROTOS	= $(PROTO_DIR)/river-layer-shell-v1.xml $(PROTO_DIR)/river-window-management-v1.xml $(PROTO_DIR)/river-xkb-bindings-v1.xml
PROTOC	= $(PROTOS:.xml=.c)
PROTOH	= $(PROTOS:.xml=.h)


# Manual targets that you would actually want to call.

jrwm	: $(CFILES) $(HFILES)
	$(CC) -o jrwm $(CFLAGS) $(CFILES)

clean	:
	rm -f jrwm $(PROTOC) $(PROTOH)

install	: jrwm
	$(MKDIR_P) $(INSTALL_DIR)
	$(INSTALL) -s jrwm $(INSTALL_DIR)

.PHONY	: clean


# XML file conversion.

.SUFFIXES: .xml .c .h

.xml.c	:
	wayland-scanner private-code $< $@

.xml.h	:
	wayland-scanner client-header $< $@
