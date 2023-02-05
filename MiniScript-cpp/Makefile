CC=gcc
CFLAGS=-c -Isrc -Isrc/editline -Isrc/whereami -Isrc/MiniScript -fsigned-char
CPPFLAGS=$(CFLAGS) -std=c++11
LIBS=-lstdc++ -lm

miniscript: src/*.h src/*.cpp src/MiniScript/*.h src/MiniScript/*.cpp src/editline/*.h src/editline/*.c src/whereami/*.c src/whereami/*.h
	$(CC) $(CFLAGS) src/editline/complete.c src/editline/editline.c src/editline/sysunix.c src/whereami/whereami.c
	$(CC) $(CPPFLAGS) src/*.cpp src/MiniScript/*.cpp
	$(CC) -o miniscript *.o $(LIBS)
	rm *.o

.PHONY: clean

install:
	chmod ugo+x miniscript
	rm -f /usr/local/bin/miniscript
	ln -s miniscript /usr/local/bin/

clean:
	rm *.o

uninstall:
	rm *.o
	rm -f miniscript /usr/local/bin/miniscript
