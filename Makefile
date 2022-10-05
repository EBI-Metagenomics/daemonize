.POSIX:

DAEMONIZE_VERSION := 0.1.0

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

dist: clean
	mkdir -p daemonize-$(DAEMONIZE_VERSION)
	cp -R LICENSE Makefile README.md argless.c argless.h daemonize.c daemonize-$(DAEMONIZE_VERSION)
	tar -cf - daemonize-$(DAEMONIZE_VERSION) | gzip > daemonize-$(DAEMONIZE_VERSION).tar.gz
	rm -rf daemonize-$(DAEMONIZE_VERSION)

distclean:
	rm -f daemonize-$(DAEMONIZE_VERSION).tar.gz

clean: distclean
	rm -f daemonize $(OBJ)

.PHONY: all clean
