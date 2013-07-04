# Makefile for testing Zygote with C++

# libZygote
ZYGOTE_PREFIX = $(realpath ../../@prefix@)
CXXFLAGS += -I$(ZYGOTE_PREFIX)/include
LDFLAGS  += -L$(ZYGOTE_PREFIX)/lib
export PATH := $(ZYGOTE_PREFIX)/bin:$(PATH)
export LD_LIBRARY_PATH := $(ZYGOTE_PREFIX)/lib:$(LD_LIBRARY_PATH)

# portability
so = so
soflag = -shared
ifeq ($(shell uname),Darwin)
    so = dylib
    soflag = -dynamiclib
    export DYLD_LIBRARY_PATH := $(ZYGOTE_PREFIX)/lib:$(DYLD_LIBRARY_PATH)
endif


TESTS = $(shell ls -d [0-9][0-9]*)
test: $(word 1,$(TESTS))/main $(TESTS:%=%/libcode.$(so))
	./test.sh $^


%/main: %/libcode.$(so) $(ZYGOTE_PREFIX)/lib/libzygote.$(so)
	$(CXX) -o $@ $(LDFLAGS) -L$(<D) -lcode -lzygote
$(ZYGOTE_PREFIX)/lib/libzygote.$(so):
	$(MAKE) install -C ../..

%/libcode.$(so): %/main.o %/myclass.o
	$(CXX) -o $@ $(soflag) $(LDFLAGS) $^ -lzygote

%.o: %.cc
	$(CXX) -o $@ -c -fPIC $(CXXFLAGS) -I. -I$(@D) $^

clean:
	rm -f */main */libcode.$(so) */*.o
