GTEST_DIR = /usr/src/gtest

CC=gcc
CXX=g++
CXXFLAGS=-Wall -g -I$(GTEST_DIR)/include $(shell pkg-config --cflags eigen3 )
LDFLAGS=-lSDL -lSDL_image -lGL -lrt -lpthread
RANLIB=ranlib

DEMO_OBJS=demo.o
TESTS=effect_chain_test

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

# Default target:
all: $(TESTS) demo

# Google Test.
GTEST_OBJS = gtest-all.o gtest_sdl_main.o

gtest-all.o: $(GTEST_DIR)/src/gtest-all.cc
	$(CXX) -MMD $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c $< -o $@
gtest_sdl_main.o: gtest_sdl_main.cpp
	$(CXX) -MMD $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c $< -o $@

# Unit tests.
effect_chain_test: effect_chain_test.o $(GTEST_OBJS) libmovit.a
	$(CXX) -o $@ effect_chain_test.o $(GTEST_OBJS) libmovit.a $(LDFLAGS)

OBJS=$(DEMO_OBJS) $(LIB_OBJS) $(GDEMO_OBJS)

# A small demo program.
demo: libmovit.a $(DEMO_OBJS)
	$(CXX) -o demo $(DEMO_OBJS) libmovit.a $(LDFLAGS)

# The library itself.
libmovit.a: $(LIB_OBJS)
	$(AR) rc $@ $(LIB_OBJS)
	$(RANLIB) $@

%.o: %.cpp
	$(CXX) -MMD $(CPPFLAGS) $(CXXFLAGS) -o $@ -c $<

DEPS=$(OBJS:.o=.d)
-include $(DEPS)

clean:
	$(RM) demo $(TESTS) libmovit.a $(OBJS) $(DEPS)

check: $(TESTS)
	for TEST in $(TESTS); do \
	    ./$$TEST; \
	done

.PHONY: clean check all
