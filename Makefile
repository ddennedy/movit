CC=gcc
CXX=g++
CXXFLAGS=-Wall -g $(shell pkg-config --cflags eigen3 )
LDFLAGS=-lSDL -lSDL_image -lGL -lrt
RANLIB=ranlib

TEST_OBJS=main.o

# Core.
LIB_OBJS=util.o widgets.o effect.o effect_chain.o

# Inputs.
LIB_OBJS += flat_input.o
LIB_OBJS += ycbcr_input.o

# Effects.
LIB_OBJS += lift_gamma_gain_effect.o
LIB_OBJS += white_balance_effect.o
LIB_OBJS += gamma_expansion_effect.o
LIB_OBJS += gamma_compression_effect.o
LIB_OBJS += colorspace_conversion_effect.o
LIB_OBJS += saturation_effect.o
LIB_OBJS += vignette_effect.o
LIB_OBJS += mirror_effect.o
LIB_OBJS += blur_effect.o
LIB_OBJS += diffusion_effect.o
LIB_OBJS += glow_effect.o
LIB_OBJS += unsharp_mask_effect.o
LIB_OBJS += mix_effect.o
LIB_OBJS += resize_effect.o
LIB_OBJS += deconvolution_sharpen_effect.o
LIB_OBJS += sandbox_effect.o

OBJS=$(TEST_OBJS) $(LIB_OBJS)

# A small test program (not a unit test).
test: libmovit.a $(TEST_OBJS)
	$(CXX) -o test $(TEST_OBJS) libmovit.a $(LDFLAGS)

# The library itself.
libmovit.a: $(LIB_OBJS)
	$(AR) rc $@ $(LIB_OBJS)
	$(RANLIB) $@

%.o: %.cpp
	$(CXX) -MMD $(CPPFLAGS) $(CXXFLAGS) -o $@ -c $<

DEPS=$(OBJS:.o=.d)
-include $(DEPS)

clean:
	$(RM) test libmovit.a $(OBJS) $(DEPS)

.PHONY: clean
