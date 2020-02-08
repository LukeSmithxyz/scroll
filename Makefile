CFLAGS += -std=c99 -pedantic -Wall -Wextra -g
LDLIBS += -lutil
CPPFLAGS += -D_DEFAULT_SOURCE

.PHONY: all clean test

all: scroll
clean:
	rm -f scroll

test: scroll
	# return code passthrough of childs
	if ! ./scroll true;  then exit 1; fi
	if   ./scroll false; then exit 1; fi
