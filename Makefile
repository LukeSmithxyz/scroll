include config.mk

.PHONY: all clean install test

all: scroll
clean:
	rm -f scroll ptty

config.h:
	cp config.def.h config.h

scroll: scroll.c config.h
	$(CC) $(CFLAGS) $(CPPFLAGS) scroll.c $(LDLIBS) -o $@

install: scroll
	cp scroll ${BINDIR}
	cp scroll.1 ${MAN1DIR}

test: scroll ptty
	# check usage
	if ./ptty ./scroll; then exit 1; fi
	# check exit passthrough of child
	if ! ./ptty ./scroll true;  then exit 1; fi
	if   ./ptty ./scroll false; then exit 1; fi
