CC ?= gcc
AR ?= ar
RANLIB ?= ranlib
STRIP ?= strip

CFLAGS += -I.

SRC = src/*.c

shorkres: $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o shorkres $(LDFLAGS)
	$(STRIP) shorkres

PREFIX ?= /usr
BINDIR = $(PREFIX)/bin

install: shorkres
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 shorkres $(DESTDIR)$(BINDIR)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/shorkres

clean:
	rm -f shorkres

.PHONY: install uninstall clean
