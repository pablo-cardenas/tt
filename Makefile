.POSIX:

include config.mk

SRC = ttcli.c ttsrv.c
OBJ = $(SRC:.c=.o)
BIN = $(SRC:.c=)

all: $(BIN)

$(OBJ): tt.h config.mk

.c.o:
	$(CC) $(TTCFLAGS) -c $<

.o:
	$(CC) -o $@ $< $(TTLDFLAGS)

clean: 
	rm -f $(OBJ) $(BIN) tt-$(VERSION).tar.gz

dist: clean
	mkdir -p tt-$(VERSION)
	cp -R README.md config.mk Makefile tt.h $(SRC) tt-$(VERSION)
	tar -czf tt-$(VERSION).tar.gz tt-$(VERSION)
	rm -rf tt-$(VERSION)

install:
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin

uninstall:
	cd $(DESTDIR)$(PREFIX)/bin; rm -f $(BIN)

.PHONY: all clean dist install uninstall
