CC=gcc
CXX=g++
CXXFLAGS=-Wall -g
LDFLAGS=-lSDL -lSDL_image -lGL

# Core.
OBJS=main.o util.o widgets.o effect.o effect_chain.o

# Inputs.
OBJS += flat_input.o
OBJS += ycbcr_input.o

# Effects.
OBJS += lift_gamma_gain_effect.o
OBJS += gamma_expansion_effect.o
OBJS += gamma_compression_effect.o
OBJS += colorspace_conversion_effect.o
OBJS += saturation_effect.o
OBJS += vignette_effect.o
OBJS += mirror_effect.o
OBJS += blur_effect.o
OBJS += diffusion_effect.o
OBJS += glow_effect.o
OBJS += mix_effect.o
OBJS += resize_effect.o
OBJS += sandbox_effect.o

test: $(OBJS)
	$(CXX) -o test $(OBJS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) -MMD $(CPPFLAGS) $(CXXFLAGS) -o $@ -c $<

DEPS=$(OBJS:.o=.d)
-include $(DEPS)

clean:
	$(RM) test $(OBJS) $(DEPS)

.PHONY: clean
