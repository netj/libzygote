# Makefile for libzygote
# Author: Jaeho Shin <netj@cs.stanford.edu>
# Created: 2013-02-05

PREFIX = @prefix@
CFLAGS += -Wall -fPIC

soext = so
soflag = -shared
ifeq ($(shell uname),Darwin)
    soext = dylib
    soflag = -dynamiclib
    LDFLAGS += -
endif

all: libzygote.$(soext) grow

install: all
	mkdir -p $(PREFIX)/{bin,lib,include}
	install -m a+rx grow               $(PREFIX)/bin/
	install -m a+rx libzygote.$(soext) $(PREFIX)/lib/
	install -m a+r  zygote.h           $(PREFIX)/include/

clean:
	rm -f *.o libzygote.$(soext) grow
	rm -f test/{example{,-{zygote,run.so}},input_file,zygote.socket}

libzygote.$(soext): zygote.o
	$(CC) -o $@ $(soflag) $^ -ldl

grow: grow.o
	$(CC) -o $@ $^

test: install
	bash test/test-example.sh

.PHONY: all install clean test
