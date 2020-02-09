include config.mk

.PHONY: all clean install test

all: scroll
clean:
	rm -f scroll

install: scroll
	cp scroll ${BINDIR}

test: scroll
	# return code passthrough of childs
	if ! ./scroll true;  then exit 1; fi
	if   ./scroll false; then exit 1; fi
