PREFIX ?= /usr
BINPREFIX ?= $(PREFIX)/bin
gd-watcher: gdbus_watcher.c
	$(CC) -o $@ $^ -Wall -pedantic `pkg-config --cflags --libs gio-2.0`

clean:
	rm gd-watcher

install: gd-watcher
	mkdir -p "$(DESTDIR)$(BINPREFIX)"
	cp -p gd-watcher "$(DESTDIR)$(BINPREFIX)"

uninstall:
	rm -f "$(DESTDIR)$(BINPREFIX)/gd-watcher"
