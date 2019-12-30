CFLAGS = -std=c99 -pedantic -Wall -Wextra
LDFLAGS += -lutil

.PHONY: all clean

all: scroll
clean:
	rm -f scroll
