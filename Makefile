ifeq ($(OS),Windows_NT)
  $(error Windows not supported)
endif

OS_NAME := $(shell uname -s | tr '[:upper:]' '[:lower:]')

ifeq ($(OS_NAME),linux)
  CC := gcc
  CFLAGS := -std=c11 -Wall -Wextra -g -Iinclude
  LDFLAGS_COMMON := -lm -lpthread -ldl
  LDFLAGS_CLIENT := -Llib -lraylib -lGL -lX11 $(LDFLAGS_COMMON)
  LDFLAGS_SERVER := $(LDFLAGS_COMMON)
else ifeq ($(OS_NAME),darwin)
  CC := clang
  CFLAGS := -std=c11 -Wall -Wextra -g -Iinclude
  LDFLAGS_COMMON := -lm -lpthread -ldl
  LDFLAGS_CLIENT := -Llib -lraylib -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL $(LDFLAGS_COMMON)
  LDFLAGS_SERVER := $(LDFLAGS_COMMON)
else
  $(error OS unsupported)
endif

OUT_DIR := out

.PHONY: all clean

server: $(OUT_DIR)/server.o $(OUT_DIR)/data.o $(OUT_DIR)/worldgen.o $(OUT_DIR)/message.o
	$(CC) $^ $(LDFLAGS_SERVER) -o $@

client: $(OUT_DIR)/client.o $(OUT_DIR)/data.o $(OUT_DIR)/resources.o $(OUT_DIR)/message.o $(OUT_DIR)/camera.o
	$(CC) $^ $(LDFLAGS_CLIENT) -o $@

worldgendisplayer: $(OUT_DIR)/worldgendisplayer.o $(OUT_DIR)/worldgen.o $(OUT_DIR)/data.o
	$(CC) $^ $(LDFLAGS_CLIENT) -o $@

$(OUT_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# $(OUT_DIR)/server.o: server.c data.h worldgen.h message.h
# 	$(CC) $(CFLAGS) -c $< -o $@

# $(OUT_DIR)/client.o: client.c utils.h data.h resources.h message.h
# 	$(CC) $(CFLAGS) -c $< -o $@

# $(OUT_DIR)/resources.o: resources.c data.h
# 	$(CC) $(CFLAGS) -c $< -o $@

# $(OUT_DIR)/data.o: data.c
# 	$(CC) $(CFLAGS) -c $< -o $@

# $(OUT_DIR)/worldgendisplayer.o: worldgendisplayer.c worldgen.h data.h
# 	$(CC) $(CFLAGS) -c $< -o $@

# $(OUT_DIR)/worldgen.o: worldgen.c data.h
# 	$(CC) $(CFLAGS) -c $< -o $@

# $(OUT_DIR)/message.o: message.c
# 	$(CC) $(CFLAGS) -c $< -o $@

# $(OUT_DIR)/utils.o: utils.c
# 	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OUT_DIR)/*.o server client worldgendisplayer
