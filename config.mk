# paths
PREFIX	= /usr/local
BINDIR	= ${PREFIX}/bin

CC ?= cc
CFLAGS = -std=c99 -pedantic -Wall -Wextra -g
LDLIBS += -lutil
CPPFLAGS += -D_DEFAULT_SOURCE
