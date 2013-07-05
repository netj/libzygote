# Makefile for building Zygote test with C++

TESTS = $(shell ls -d [0-9][0-9]*)
all: $(word 1,$(TESTS))/main $(TESTS:%=%/libcode.$(so))

%/main: %/libcode.$(so)
	$(CXX) -o $@ $(LDFLAGS) -L$(<D) -lcode $(LIBS)

%/libcode.$(so): %/main.o %/myclass.o
	$(CXX) -o $@ $(sharedflag) $(LDFLAGS) $^ $(LIBS)

%.o: %.cc
	$(CXX) -o $@ -c -fPIC $(CXXFLAGS) -I. -I$(@D) $^

clean:
	rm -f */main */libcode.$(so) */*.o
