# Basic make variables.

CC	= gcc
INSTALL	= install -c -s
MKDIR_P	= mkdir -p

PREFIX	= /usr/local
BINDIR	= $(PREFIX)/bin
MANDIR	= $(PREFIX)/man

CFLAGS	?= -ggdb -O0 -Wall
CFLAGS	+= -I. -I$(PROTODIR)
#CFLAGS	+= -std=c99 -pedantic -D_POSIX_C_SOURCE=200112L
LDFLAGS	?= -flto
LDFLAGS	+= -lwayland-client -lxkbcommon

CONFIG	= config.c
CFILES	= jrwm.c layout.c bindings.c $(CONFIG) $(PROTOC)
OFILES	= $(CFILES:.c=.o)
HFILES	= jrwm.h $(PROTOH)
PROTODIR = ./protocol


# Generated file variables.

PROTOS	= $(PROTODIR)/river-layer-shell-v1.xml $(PROTODIR)/river-window-management-v1.xml $(PROTODIR)/river-xkb-bindings-v1.xml
PROTOC	= $(PROTOS:.xml=.c)
PROTOH	= $(PROTOS:.xml=.h)


# Manual targets that you would actually want to call.

jrwm	: $(OFILES)
	$(CC) -o jrwm $(CFLAGS) $(OFILES) $(LDFLAGS)
$(OFILES)	: $(HFILES)

clean	:
	rm -f jrwm $(PROTOC) $(OFILES) $(PROTOH)

install	: jrwm
	$(MKDIR_P) $(BINDIR)
	$(INSTALL) jrwm $(BINDIR)
	$(MKDIR_P) $(MANDIR)/man1
	cp doc/jrwm.1 $(MANDIR)/man1
	chmod 644 $(MANDIR)/man1/jrwm.1

.PHONY	: clean


# XML file conversion.

.SUFFIXES: .xml .c .h

.c.o:
	$(CC) -c $< -o $@ $(CFLAGS)

.xml.c	:
	wayland-scanner private-code $< $@

.xml.h	:
	wayland-scanner client-header $< $@
