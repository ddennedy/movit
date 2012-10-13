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

void expect_equal(const float *ref, const float *result, unsigned width, unsigned height, float largest_difference_limit = 1.5 / 255.0, float rms_limit = 0.2 / 255.0);

#endif  // !defined(_TEST_UTIL_H)
