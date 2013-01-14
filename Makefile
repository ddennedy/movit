GTEST_DIR = /usr/src/gtest

EIGEN_CXXFLAGS := $(shell pkg-config --cflags eigen3)
ifeq ($(EIGEN_CXXFLAGS),)
$(error Empty EIGEN_CXXFLAGS. You probably need to install Eigen3)
endif

GLEW_CXXFLAGS := $(shell pkg-config --cflags glew)
ifeq ($(GLEW_CXXFLAGS),)
$(error Empty GLEW_CXXFLAGS. You probably need to install GLEW)
endif

GLEW_LIBS := $(shell pkg-config --libs glew)
ifeq ($(GLEW_LIBS),)
$(error Empty GLEW_LIBS. You probably need to install GLEW)
endif

CC=gcc
CXX=g++
CXXFLAGS=-Wall -g -I$(GTEST_DIR)/include $(EIGEN_CXXFLAGS) $(GLEW_CXXFLAGS)
LDFLAGS=-lSDL -lSDL_image -lGL -lrt -lpthread $(GLEW_LIBS)
RANLIB=ranlib

ifeq ($(COVERAGE),1)
CXXFLAGS += -fprofile-arcs -ftest-coverage
LDFLAGS += -fprofile-arcs -ftest-coverage
endif

DEMO_OBJS=demo.o

# Unit tests.
TESTS=effect_chain_test
TESTS += mix_effect_test
TESTS += overlay_effect_test
TESTS += gamma_expansion_effect_test
TESTS += gamma_compression_effect_test
TESTS += colorspace_conversion_effect_test
TESTS += alpha_multiplication_effect_test
TESTS += alpha_division_effect_test
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
LIB_OBJS += alpha_multiplication_effect.o
LIB_OBJS += alpha_division_effect.o
LIB_OBJS += saturation_effect.o
LIB_OBJS += vignette_effect.o
LIB_OBJS += mirror_effect.o
LIB_OBJS += blur_effect.o
LIB_OBJS += diffusion_effect.o
LIB_OBJS += glow_effect.o
LIB_OBJS += unsharp_mask_effect.o
LIB_OBJS += mix_effect.o
LIB_OBJS += overlay_effect.o
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
$(TESTS): %: %.o $(TEST_OBJS) libmovit.a
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
	$(CXX) -MMD -MP $(CPPFLAGS) $(CXXFLAGS) -o $@ -c $<

DEPS=$(OBJS:.o=.d)
-include $(DEPS)

clean:
	$(RM) demo $(TESTS) libmovit.a $(OBJS) $(OBJS:.o=.gcno) $(OBJS:.o=.gcda) $(DEPS) step-*.dot
	$(RM) -r movit.info coverage/

check: $(TESTS)
	FAILED_TESTS=""; \
	for TEST in $(TESTS); do \
	    ./$$TEST || FAILED_TESTS="$$TEST $$FAILED_TESTS"; \
	done; \
	if [ "$$FAILED_TESTS" ]; then \
		echo Failed tests: $$FAILED_TESTS; \
		exit 1; \
	fi

# You need to build with COVERAGE=1 to use this target.
coverage: check
	lcov -d . -c -o movit.info
	lcov --remove movit.info '*_test.cpp' '*/test_util.{cpp,h}' -o movit.info
	genhtml -o coverage movit.info

.PHONY: coverage clean check all
