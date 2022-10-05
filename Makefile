.POSIX:

ARGLESS_VERSION := 0.1.0

CC := gcc
CFLAGS := $(CFLAGS) -std=c11 -Wall -Wextra

SRC := daemonize.c argless.c
OBJ := $(SRC:.c=.o)

all: daemonize

%.o: %.c $(HDR)
	$(CC) $(CFLAGS) -c $<

argless.o: argless.h
daemonize.o: argless.h

daemonize: $(OBJ)
	$(CC) -o $@ $(OBJ)

clean:
	rm -f st $(OBJ)

.PHONY: all clean
