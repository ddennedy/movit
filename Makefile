CC=gcc
CXX=g++
CXXFLAGS=-Wall
LDFLAGS=-lSDL -lSDL_image -lGL

test: test.o util.o
	$(CXX) -o test test.o util.o $(LDFLAGS)

.o: .cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $<

clean:
	$(RM) test test.o util.o

.PHONY: clean
