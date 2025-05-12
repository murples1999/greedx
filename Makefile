PREFIX ?= /usr/local

all:
	clang -o greedx greed.c -lncurses

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp greedx $(DESTDIR)$(PREFIX)/bin/
