# tt version
VERSION = 0.1.0

# Customize below to fit your system

# paths
PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

PKG_CONFIG = pkg-config

# includes and libs
INCS = `$(PKG_CONFIG) --cflags ncurses` \
       `$(PKG_CONFIG) --cflags json-c`
LIBS = `$(PKG_CONFIG) --libs ncurses` \
       `$(PKG_CONFIG) --libs json-c`

# flags
TTCPPFLAGS = -DVERSION=\"$(VERSION)\" $(CPPFLAGS)
TTCFLAGS = $(INCS) $(CPPFLAGS) $(CFLAGS)
TTLDFLAGS = $(LIBS) $(LDFLAGS)
