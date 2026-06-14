CC = gcc
CFLAGS = -g -Wall $(shell pkg-config --cflags libevdev)
LDFLAGS = $(shell pkg-config --libs libevdev)

key-remap: key-remap.o


compile_flags.txt: Makefile
	printf '%s\n' $(CFLAGS) > $@
