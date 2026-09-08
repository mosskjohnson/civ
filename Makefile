ifeq ($(OS),Windows_NT)
  $(error Windows not supported)
endif

OS_NAME := $(shell uname -s | tr '[:upper:]' '[:lower:]')

ifeq ($(OS_NAME),linux)
  CC := gcc
  CFLAGS := -std=gnu11 -Wall -Wextra -g -Iinclude
  LDFLAGS := -lm -lpthread -lrt -ldl -Llib -lraylib -lGL -lX11
else ifeq ($(OS_NAME),darwin)
  CC := clang
  CFLAGS := -std=gnu11 -Wall -Wextra -g -Iinclude
  LDFLAGS := -lm -lpthread -ldl -Llib -lraylib -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL
else
  $(error OS unsupported)
endif

SRC_DIR := src
BUILD_DIR := build
BIN_DIR := bin

.PHONY: all clean

all: server client worldgendisplayer

server: $(BUILD_DIR)/server.o $(BUILD_DIR)/data.o $(BUILD_DIR)/worldgen.o $(BUILD_DIR)/message.o
	$(CC) $^ $(LDFLAGS) -o $(BIN_DIR)/$@

client: $(BUILD_DIR)/client.o $(BUILD_DIR)/data.o $(BUILD_DIR)/resources.o $(BUILD_DIR)/message.o $(BUILD_DIR)/camera.o
	$(CC) $^ $(LDFLAGS) -o $(BIN_DIR)/$@

worldgendisplayer: $(BUILD_DIR)/worldgendisplayer.o $(BUILD_DIR)/worldgen.o $(BUILD_DIR)/data.o $(BUILD_DIR)/resources.o
	$(CC) $^ $(LDFLAGS) -o $(BIN_DIR)/$@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(BUILD_DIR)/* $(BIN_DIR)/*
