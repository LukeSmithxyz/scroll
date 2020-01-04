CFLAGS += -std=c99 -pedantic -Wall -Wextra
LDLIBS += -lutil
CPPFLAGS += -D_DEFAULT_SOURCE

.PHONY: all clean

all: scroll
clean:
	rm -f scroll
