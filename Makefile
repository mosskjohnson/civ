ifeq ($(OS),Windows_NT)
    OS_NAME := windows
else
    OS_NAME := $(shell uname -s | tr '[:upper:]' '[:lower:]')
endif

#default linux
CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -g -Iinclude
LDFLAGS := -Llib -lraylib -lm -lpthread -ldl

ifeq ($(OS_NAME),darwin)
	#macOS
	CC := clang
	LDFLAGS := -Llib -lraylib -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL
endif

.PHONY: all clean

all: server client

server: server.o
	$(CC) $(CFLAGS) $^ -o $@

server.o: server.c
	$(CC) $(CFLAGS) -c $< -o $@

client: client.o data.o resources.o
	$(CC) $^ $(LDFLAGS) -o $@

client.o: client.c data.h resources.h
	$(CC) $(CFLAGS) -c $< -o $@

resources.o: resources.c data.h
	$(CC) $(CFLAGS) -c $< -o $@

data.o: data.c
	$(CC) $(CFLAGS) -c $< -o $@
	
clean:
	rm -f *.o server client
