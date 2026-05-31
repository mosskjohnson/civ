ifeq ($(OS),Windows_NT)
    OS_NAME := windows
else
    OS_NAME := $(shell uname -s | tr '[:upper:]' '[:lower:]')
endif

#default linux
CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -g -Iinclude
LDFLAGS := -Llib -lraylib -lm -lpthread -ldl -lGL -lX11

ifeq ($(OS_NAME),darwin)
	#macOS
	CC := clang
	LDFLAGS := -Llib -lraylib -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL
endif

.PHONY: all clean

server: server.o
	$(CC) $^ -o $@

client: client.o data.o resources.o
	$(CC) $^ $(LDFLAGS) -o $@

worldgendisplayer: worldgendisplayer.o worldgen.o data.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

server.o: server.c
	$(CC) $(CFLAGS) -c $< -o $@

client.o: client.c data.h resources.h
	$(CC) $(CFLAGS) -c $< -o $@

resources.o: resources.c data.h
	$(CC) $(CFLAGS) -c $< -o $@

data.o: data.c
	$(CC) $(CFLAGS) -c $< -o $@

worldgendisplayer.o: worldgendisplayer.c worldgen.h data.h
	$(CC) -c $< -o $@

worldgen.o: worldgen.c data.h
	$(CC) $(CFLAGS) -c $< -o $@
	
clean:
	rm -f *.o server client worldgendisplayer
