CC ?= gcc
AR ?= ar
RANLIB ?= ranlib
STRIP ?= strip

CFLAGS += -D_GNU_SOURCE -std=gnu99 -I. -Os
LDFLAGS += -static

ifdef FB
	CFLAGS += -DFB
endif

SRC = src/*.c

shorkset: $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o shorkset $(LDFLAGS)
	$(STRIP) shorkset

PREFIX ?= /usr
BINDIR = $(PREFIX)/libexec
CONFDIR = /etc
DATDIR = $(PREFIX)/share/shorkset

install: shorkset
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 shorkset $(DESTDIR)$(BINDIR)

	install -d $(DESTDIR)$(CONFDIR)
	install -m 644 shorkset.conf $(DESTDIR)$(CONFDIR)

	install -d $(DESTDIR)$(DATDIR)
	install -m 644 modules.csv $(DESTDIR)$(DATDIR)
	install -m 644 module-devs.csv $(DESTDIR)$(DATDIR)
uninstall:
	rm -f $(DESTDIR)$(BINDIR)/shorkset
	rm -f $(DESTDIR)$(CONFDIR)/shorkset.conf
	rm -f $(DESTDIR)$(DATDIR)/modules.csv
	rm -f $(DESTDIR)$(DATDIR)/module-devs.csv

clean:
	rm -f shorkset

.PHONY: install uninstall clean
