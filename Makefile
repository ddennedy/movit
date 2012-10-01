CC=gcc
CFLAGS=-std=gnu99 -Wall
LDFLAGS=-lSDL -lSDL_image -lGL

test: test.o
	$(CC) -o test test.o $(LDFLAGS)

.o: .c
	$(CC) $(CXXFLAGS) $(CCFLAGS) -o $@ $<

clean:
	$(RM) test test.o

.PHONY: clean
