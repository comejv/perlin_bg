# --- PKG_CONFIG / FFMPEG setup ---
PKG_CONFIG   ?= pkg-config
FFMPEG_NAMES := libavformat libavcodec libavutil libswscale
FFMPEG_CFLAGS:= $(shell $(PKG_CONFIG) --cflags  $(FFMPEG_NAMES))
FFMPEG_LIBS  := $(shell $(PKG_CONFIG) --libs    $(FFMPEG_NAMES))

CC      ?= gcc
CFLAGS  ?= -O3 -Wall -Wextra -pedantic -fopenmp -isystem external/
LDFLAGS ?= -fopenmp

# --- Gather sources & headers ---
SRC    := $(wildcard src/*.c)
HDR    := $(wildcard headers/*.h)

# exclude each main‐file from the other build:
GUI_SRCS := $(filter-out src/main_cli.c,$(SRC))
CLI_SRCS := $(filter-out src/main.c,    $(SRC))

# map .c -> build/%.o
GUI_OBJS := $(patsubst src/%.c,build/%.o,$(GUI_SRCS))
CLI_OBJS := $(patsubst src/%.c,build/%.o,$(CLI_SRCS))

.PHONY: all clean
all: main main_cli

# compile rule
build/%.o: src/%.c $(HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# link GUI version (raylib)
main: $(GUI_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ -lm -lraylib $(FFMPEG_LIBS)

# link CLI version (ffmpeg)
main_cli: $(CLI_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ -lm $(FFMPEG_LIBS)

clean:
	rm -rf main main_cli build
