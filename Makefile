CC = cc
CFLAGS=-O2 -fPIC -fstack-protector-strong -D_GNU_SOURCE -s -z norelro
PREFIX=/usr
BINDIR=$(PREFIX)/bin

all: tinyionice

tinyionice: main.c
	$(CC) $(CFLAGS) $< -o $@

install: tinyionice
	install -D -m 755 tinyionice $(DESTDIR)/$(BINDIR)/tinyionice

uninstall:
	rm -f $(DESTDIR)/$(BINDIR)/tinyionice

clean:
	rm -f tinyionice


.PHONY: all install uninstall clean
