CFLAGS += -std=c99 -pedantic -Wall -Wextra
LDLIBS += -lutil
CPPFLAGS += -D_DEFAULT_SOURCE

.PHONY: all clean test

all: scroll
clean:
	rm -f scroll

test: scroll
	# return code passthrough of childs
	./scroll /usr/bin/true  && if [ $$? -ne 0 ]; then exit 1; fi
	./scroll /usr/bin/false || if [ $$? -ne 1 ]; then exit 1; fi
