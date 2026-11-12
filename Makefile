CC := gcc
CFLAGS := -Wall -Wextra -Wpedantic $(shell pkg-config --cflags libevdev)
LDFLAGS := $(shell pkg-config --libs libevdev)

.PHONY := all

all: key-remap
