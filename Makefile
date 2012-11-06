GTEST_DIR = /usr/src/gtest

CC=gcc
CXX=g++
CXXFLAGS=-Wall -g -I$(GTEST_DIR)/include $(shell pkg-config --cflags eigen3 )
LDFLAGS=-lSDL -lSDL_image -lGL -lrt -lpthread
RANLIB=ranlib

ifeq ($(COVERAGE),1)
CXXFLAGS += -fprofile-arcs -ftest-coverage
LDFLAGS += -fprofile-arcs -ftest-coverage
endif

DEMO_OBJS=demo.o

# Unit tests.
TESTS=effect_chain_test
TESTS += mix_effect_test
TESTS += gamma_expansion_effect_test
TESTS += gamma_compression_effect_test
TESTS += colorspace_conversion_effect_test
TESTS += saturation_effect_test
TESTS += deconvolution_sharpen_effect_test
TESTS += blur_effect_test
TESTS += unsharp_mask_effect_test
TESTS += diffusion_effect_test
TESTS += white_balance_effect_test
TESTS += lift_gamma_gain_effect_test
TESTS += resample_effect_test
TESTS += dither_effect_test
TESTS += flat_input_test
TESTS += ycbcr_input_test

# Core.
LIB_OBJS=util.o widgets.o effect.o effect_chain.o init.o

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
LIB_OBJS += resample_effect.o
LIB_OBJS += dither_effect.o
LIB_OBJS += deconvolution_sharpen_effect.o
LIB_OBJS += sandbox_effect.o

# Default target:
all: $(TESTS) demo

# Google Test and other test library functions.
TEST_OBJS = gtest-all.o gtest_sdl_main.o test_util.o

gtest-all.o: $(GTEST_DIR)/src/gtest-all.cc
	$(CXX) -MMD $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c $< -o $@
gtest_sdl_main.o: gtest_sdl_main.cpp
	$(CXX) -MMD $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c $< -o $@

# Unit tests.
effect_chain_test: effect_chain_test.o $(TEST_OBJS) libmovit.a
	$(CXX) -o $@ $^ $(LDFLAGS)
mix_effect_test: mix_effect_test.o $(TEST_OBJS) libmovit.a
	$(CXX) -o $@ $^ $(LDFLAGS)
gamma_expansion_effect_test: gamma_expansion_effect_test.o $(TEST_OBJS) libmovit.a
	$(CXX) -o $@ $^ $(LDFLAGS)
gamma_compression_effect_test: gamma_compression_effect_test.o $(TEST_OBJS) libmovit.a
	$(CXX) -o $@ $^ $(LDFLAGS)
colorspace_conversion_effect_test: colorspace_conversion_effect_test.o $(TEST_OBJS) libmovit.a
	$(CXX) -o $@ $^ $(LDFLAGS)
saturation_effect_test: saturation_effect_test.o $(TEST_OBJS) libmovit.a
	$(CXX) -o $@ $^ $(LDFLAGS)
deconvolution_sharpen_effect_test: deconvolution_sharpen_effect_test.o $(TEST_OBJS) libmovit.a
	$(CXX) -o $@ $^ $(LDFLAGS)
blur_effect_test: blur_effect_test.o $(TEST_OBJS) libmovit.a
	$(CXX) -o $@ $^ $(LDFLAGS)
unsharp_mask_effect_test: unsharp_mask_effect_test.o $(TEST_OBJS) libmovit.a
	$(CXX) -o $@ $^ $(LDFLAGS)
diffusion_effect_test: diffusion_effect_test.o $(TEST_OBJS) libmovit.a
	$(CXX) -o $@ $^ $(LDFLAGS)
white_balance_effect_test: white_balance_effect_test.o $(TEST_OBJS) libmovit.a
	$(CXX) -o $@ $^ $(LDFLAGS)
lift_gamma_gain_effect_test: lift_gamma_gain_effect_test.o $(TEST_OBJS) libmovit.a
	$(CXX) -o $@ $^ $(LDFLAGS)
resample_effect_test: resample_effect_test.o $(TEST_OBJS) libmovit.a
	$(CXX) -o $@ $^ $(LDFLAGS)
dither_effect_test: dither_effect_test.o $(TEST_OBJS) libmovit.a
	$(CXX) -o $@ $^ $(LDFLAGS)
flat_input_test: flat_input_test.o $(TEST_OBJS) libmovit.a
	$(CXX) -o $@ $^ $(LDFLAGS)
ycbcr_input_test: ycbcr_input_test.o $(TEST_OBJS) libmovit.a
	$(CXX) -o $@ $^ $(LDFLAGS)

OBJS=$(DEMO_OBJS) $(LIB_OBJS) $(TEST_OBJS) $(TESTS:=.o)

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
	$(RM) demo $(TESTS) libmovit.a $(OBJS) $(OBJS:.o=.gcno) $(OBJS:.o=.gcda) $(DEPS) step-*.dot
	$(RM) -r movit.info coverage/

check: $(TESTS)
	FAIL=0; \
	for TEST in $(TESTS); do \
	    ./$$TEST || FAIL=1; \
	done; \
	exit $$FAIL

# You need to build with COVERAGE=1 to use this target.
coverage: check
	lcov -d . -c -o movit.info
	lcov --remove movit.info '*_test.cpp' '*/test_util.{cpp,h}' -o movit.info
	genhtml -o coverage movit.info

.PHONY: coverage clean check all
