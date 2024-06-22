CFLAGS = -g
LDFLAGS = `pkg-config --libs ncurses` `pkg-config --libs json-c`

all: ttcli ttsrv

ttcli.o: tt.h

ttsrv.o: tt.h

clean: 
	rm -f ttcli.o ttcli ttsrv.o ttsrv

.PHONY: all clean
