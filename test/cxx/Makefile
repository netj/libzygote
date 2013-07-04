# Makefile for testing Zygote with C++

ZYGOTE_PREFIX = $(realpath ../../@prefix@)
CXXFLAGS += -I$(ZYGOTE_PREFIX)/include
LDFLAGS  += -L$(ZYGOTE_PREFIX)/lib
export PATH := $(ZYGOTE_PREFIX)/bin:$(PATH)
export LD_LIBRARY_PATH := $(ZYGOTE_PREFIX)/lib:$(LD_LIBRARY_PATH)

TESTS = $(shell ls -d [0-9][0-9]*)

test: $(word 1,$(TESTS))/main $(TESTS:%=%/libcode.so)
	./test.sh $^


%/main: %/libcode.so $(ZYGOTE_PREFIX)/lib/libzygote.so
	$(CXX) -o $@ $(LDFLAGS) -L$(<D) -lcode -lzygote
$(ZYGOTE_PREFIX)/lib/libzygote.so:
	$(MAKE) install -C ../..

%/libcode.so: %/main.o %/myclass.o
	$(CXX) -o $@ -shared $(LDFLAGS) $^

%.o: %.cc
	$(CXX) -o $@ -c -fPIC $(CXXFLAGS) -I. -I$(@D) $^

clean:
	rm -f */main */code.so */*.o
