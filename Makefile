CC=gcc
CXX=g++
CXXFLAGS=-Wall
LDFLAGS=-lSDL -lSDL_image -lGL
OBJS=main.o util.o widgets.o effect.o

test: $(OBJS)
	$(CXX) -o test $(OBJS) $(LDFLAGS)

.o: .cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $<

clean:
	$(RM) test $(OBJS)

.PHONY: clean
