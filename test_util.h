#ifndef _TEST_UTIL_H
#define _TEST_UTIL_H 1

#include "effect_chain.h"

class EffectChainTester {
public:
	EffectChainTester(const float *data, unsigned width, unsigned height, MovitPixelFormat pixel_format, ColorSpace color_space, GammaCurve gamma_curve);
	EffectChain *get_chain() { return &chain; }
	Input *add_input(const float *data, MovitPixelFormat pixel_format, ColorSpace color_space, GammaCurve gamma_curve);
	void run(float *out_data, GLenum format, ColorSpace color_space, GammaCurve gamma_curve);

private:
	EffectChain chain;
	GLuint fbo, texnum;
	unsigned width, height;
};

void expect_equal(const float *ref, const float *result, unsigned width, unsigned height);

#endif  // !defined(_TEST_UTIL_H)
