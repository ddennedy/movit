CC=gcc
CXX=g++
CFLAGS=-std=gnu99 -Wall
LDFLAGS=-lSDL -lSDL_image -lGL

test: test.o
	$(CXX) -o test test.o $(LDFLAGS)

.o: .cpp
	$(CXX) $(CXXFLAGS) $(CCFLAGS) -o $@ $<

clean:
	$(RM) test test.o

.PHONY: clean
