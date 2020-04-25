.POSIX:

include config.mk

all: scroll

config.h:
	cp config.def.h config.h

scroll: scroll.c config.h

install: scroll
	mkdir -p $(BINDIR) $(MANDIR)/man1
	cp -f scroll $(BINDIR)
	cp -f scroll.1 $(MANDIR)/man1

test: scroll ptty
	# check usage
	if ./ptty ./scroll -h; then exit 1; fi
	# check exit passthrough of child
	if ! ./ptty ./scroll true;  then exit 1; fi
	if   ./ptty ./scroll false; then exit 1; fi
	./up.sh

clean:
	rm -f scroll ptty

distclean: clean
	rm -f config.h scroll-$(VERSION).tar.gz

dist: clean
	mkdir -p scroll-$(VERSION)
	cp -R README scroll.1 TODO Makefile config.mk config.def.h \
		ptty.c scroll.c \
		scroll-$(VERSION)
	tar -cf - scroll-$(VERSION) | gzip > scroll-$(VERSION).tar.gz
	rm -rf scroll-$(VERSION)

.c:
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $< -lutil

.PHONY: all install test clean distclean dist
