ifeq ($(OS),Windows_NT)
    OS_NAME := windows
else
    OS_NAME := $(shell uname -s | tr '[:upper:]' '[:lower:]')
endif

#default linux
CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -g -Iinclude
LDFLAGS_COMMON := -lm -lpthread -ldl
LDFLAGS_CLIENT := -Llib -lraylib -lGL -lX11 $(LDFLAGS_COMMON)
LDFLAGS_SERVER := $(LDFLAGS_COMMON)

ifeq ($(OS_NAME),darwin)
	#macOS
	CC := clang
	CFLAGS := -std=c11 -Wall -Wextra -g -Iinclude
	LDFLAGS_COMMON := -lm -lpthread -ldl
	LDFLAGS_CLIENT := -Llib -lraylib -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL $(LDFLAGS_COMMON)
	LDFLAGS_SERVER := $(LDFLAGS_COMMON)
endif

.PHONY: all clean

server: server.o data.o worldgen.o message.o
	$(CC) $^ $(LDFLAGS_SERVER) -o $@

client: client.o data.o resources.o message.o
	$(CC) $^ $(LDFLAGS_CLIENT) -o $@

worldgendisplayer: worldgendisplayer.o worldgen.o data.o
	$(CC) $^ $(LDFLAGS_CLIENT) -o $@

server.o: server.c data.h worldgen.h message.h
	$(CC) $(CFLAGS) -c $< -o $@

client.o: client.c data.h resources.h message.h
	$(CC) $(CFLAGS) -c $< -o $@

resources.o: resources.c data.h
	$(CC) $(CFLAGS) -c $< -o $@

data.o: data.c
	$(CC) $(CFLAGS) -c $< -o $@

worldgendisplayer.o: worldgendisplayer.c worldgen.h data.h
	$(CC) $(CFLAGS) -c $< -o $@

worldgen.o: worldgen.c data.h
	$(CC) $(CFLAGS) -c $< -o $@

message.o: message.c
	$(CC) $(CFLAGS) -c $< -o $@
	
clean:
	rm -f *.o server client worldgendisplayer
