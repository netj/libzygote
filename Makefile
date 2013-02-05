# Makefile for libzygote
# Author: Jaeho Shin <netj@cs.stanford.edu>
# Created: 2013-02-05

PREFIX = @prefix@
CFLAGS += -Wall -fPIC

all: libzygote.so grow

install: all
	mkdir -p $(PREFIX)/{bin,lib,include}
	install -m a+rx grow         $(PREFIX)/bin/
	install -m a+rx libzygote.so $(PREFIX)/lib/
	install -m a+r  zygote.h     $(PREFIX)/include/

clean:
	rm -f *.o libzygote.so grow

libzygote.so: zygote.o
	$(CC) -o $@ -shared $^ -ldl

grow: grow.o
	$(CC) -o $@ $^

.PHONY: all install clean
