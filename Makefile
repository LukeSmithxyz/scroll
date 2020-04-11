.POSIX:

include config.mk

all: scroll

clean:
	rm -f scroll ptty

config.h:
	cp config.def.h config.h

scroll: config.h

install: scroll
	mkdir -p $(BINDIR) $(MANDIR)/man1
	cp -f scroll $(BINDIR)
	cp -f scroll.1 $(MANDIR)/man1

test: scroll ptty
	# check usage
	if ./ptty ./scroll; then exit 1; fi
	# check exit passthrough of child
	if ! ./ptty ./scroll true;  then exit 1; fi
	if   ./ptty ./scroll false; then exit 1; fi

.c:
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $< -lutil

.PHONY: all clean install test
