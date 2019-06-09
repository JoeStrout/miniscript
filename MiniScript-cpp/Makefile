CC=gcc
CFLAGS=-c -Isrc -Isrc/editline -Isrc/MiniScript
LIBS=-lstdc++ -lm

miniscript: src/*.h src/*.cpp src/MiniScript/*.h src/MiniScript/*.cpp src/editline/*.h src/editline/*.c
	$(CC) -o miniscript src/*.cpp src/editline/complete.c src/editline/editline.c src/editline/sysunix.c src/MiniScript/*.cpp -Isrc -Isrc/editline -Isrc/MiniScript $(LIBS)

.PHONY: clean

install:
	chmod ugo+x miniscript
	mv miniscript /usr/local/bin

clean:
	rm -f miniscript /usr/local/bin/miniscript
