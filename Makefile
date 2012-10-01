CC=gcc
CXX=g++
CXXFLAGS=-Wall
LDFLAGS=-lSDL -lSDL_image -lGL
OBJS=test.o util.o

test: $(OBJS)
	$(CXX) -o test $(OBJS) $(LDFLAGS)

.o: .cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $<

clean:
	$(RM) test $(OBJS)

.PHONY: clean
