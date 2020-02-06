CFLAGS += -std=c99 -pedantic -Wall -Wextra
LDLIBS += -lutil
CPPFLAGS += -D_DEFAULT_SOURCE

.PHONY: all clean test

all: scroll
clean:
	rm -f scroll

test: scroll
	# return code passthrough of childs
	if ! ./scroll /usr/bin/true;  then exit 1; fi
	if   ./scroll /usr/bin/false; then exit 1; fi
