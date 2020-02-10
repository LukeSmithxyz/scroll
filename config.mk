# paths
PREFIX	= /usr/local
BINDIR	= ${PREFIX}/bin
MANDIR	= ${PREFIX}/share/man
MAN1DIR	= ${MANDIR}/man1

CC ?= cc
CFLAGS = -std=c99 -pedantic -Wall -Wextra -g
LDLIBS += -lutil
CPPFLAGS += -D_DEFAULT_SOURCE
