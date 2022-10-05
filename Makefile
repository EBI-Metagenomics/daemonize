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

test check: daemonize
	rm -f stdin stdout stderr pid
	./daemonize sleep 10
	sleep 1
	tee -a >stdin &
	tail -f stdout &
	tail -f stderr &
	sleep 1
	ps -p `cat pid`
	kill `cat pid`
	rm -f stdin stdout stderr pid


dist: clean
	mkdir -p daemonize-$(DAEMONIZE_VERSION)
	cp -R LICENSE Makefile README.md argless.c argless.h daemonize.c daemonize-$(DAEMONIZE_VERSION)
	tar -cf - daemonize-$(DAEMONIZE_VERSION) | gzip > daemonize-$(DAEMONIZE_VERSION).tar.gz
	rm -rf daemonize-$(DAEMONIZE_VERSION)

distclean:
	rm -f daemonize-$(DAEMONIZE_VERSION).tar.gz

clean: distclean
	rm -f daemonize $(OBJ) stdin stdout stderr pid

.PHONY: all clean dist distclean test check
