CC=gcc
CXX=g++
CXXFLAGS=-Wall -g
LDFLAGS=-lSDL -lSDL_image -lGL
OBJS=main.o util.o widgets.o effect.o effect_chain.o
OBJS += lift_gamma_gain_effect.o gamma_expansion_effect.o gamma_compression_effect.o colorspace_conversion_effect.o saturation_effect.o vignette_effect.o mirror_effect.o blur_effect.o

test: $(OBJS)
	$(CXX) -o test $(OBJS) $(LDFLAGS)

.o: .cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $<

clean:
	$(RM) test $(OBJS)

.PHONY: clean
