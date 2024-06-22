CFLAGS = -g
LDFLAGS = `pkg-config --libs ncurses` `pkg-config --libs json-c`

all: tt ttserver

ttcli.o: tt.h

ttsrv.o: tt.h

clean: 
	rm -f tt.o tt ttserver.o ttserver

.PHONY: all clean
