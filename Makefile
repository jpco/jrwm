# Basic make variables.

CC	= gcc
INSTALL	= /usr/bin/install -c -s
MKDIR_P	= /usr/bin/mkdir -p

PREFIX	= /usr/local
BINDIR	= $(PREFIX)/bin
MANDIR	= $(PREFIX)/man

CFLAGS	= -g -O2 -Wall -I$(PROTODIR) -flto -lwayland-client -lxkbcommon  # -std=c99 -pedantic -D_POSIX_C_SOURCE=200112L

CFILES	= jrwm.c layout.c bindings.c config.c $(PROTOC)
HFILES	= jrwm.h $(PROTOH)
PROTODIR = ./protocol


# Generated file variables.

PROTOS	= $(PROTODIR)/river-layer-shell-v1.xml $(PROTODIR)/river-window-management-v1.xml $(PROTODIR)/river-xkb-bindings-v1.xml
PROTOC	= $(PROTOS:.xml=.c)
PROTOH	= $(PROTOS:.xml=.h)


# Manual targets that you would actually want to call.

jrwm	: $(CFILES) $(HFILES)
	$(CC) -o jrwm $(CFLAGS) $(CFILES)

clean	:
	rm -f jrwm $(PROTOC) $(PROTOH)

install	: jrwm
	$(MKDIR_P) $(BINDIR)
	$(INSTALL) jrwm $(BINDIR)
	$(MKDIR_P) $(MANDIR)/man1
	cp doc/jrwm.1 $(MANDIR)/man1
	chmod 644 $(MANDIR)/man1/jrwm.1

.PHONY	: clean


# XML file conversion.

.SUFFIXES: .xml .c .h

.xml.c	:
	wayland-scanner private-code $< $@

.xml.h	:
	wayland-scanner client-header $< $@
