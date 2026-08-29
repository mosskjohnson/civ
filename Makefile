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
  LDFLAGS := -lm -lpthread -lrt -ldl -Llib -lraylib -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL
else
  $(error OS unsupported)
endif

OUT_DIR := out

.PHONY: all clean

server: $(OUT_DIR)/server.o $(OUT_DIR)/data.o $(OUT_DIR)/worldgen.o $(OUT_DIR)/message.o
	$(CC) $^ $(LDFLAGS) -o $@

client: $(OUT_DIR)/client.o $(OUT_DIR)/data.o $(OUT_DIR)/resources.o $(OUT_DIR)/message.o $(OUT_DIR)/camera.o
	$(CC) $^ $(LDFLAGS) -o $@

worldgendisplayer: $(OUT_DIR)/worldgendisplayer.o $(OUT_DIR)/worldgen.o $(OUT_DIR)/data.o
	$(CC) $^ $(LDFLAGS) -o $@

$(OUT_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OUT_DIR)/*.o server client worldgendisplayer
