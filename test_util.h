#ifndef _TEST_UTIL_H
#define _TEST_UTIL_H 1

#include "effect_chain.h"

class EffectChainTester {
public:
	EffectChainTester(const float *data, unsigned width, unsigned height,
	                  MovitPixelFormat pixel_format = FORMAT_GRAYSCALE,
	                  Colorspace color_space = COLORSPACE_sRGB,
	                  GammaCurve gamma_curve = GAMMA_LINEAR,
	                  GLenum framebuffer_format = GL_RGBA16F_ARB);
	~EffectChainTester();
	
	EffectChain *get_chain() { return &chain; }
	Input *add_input(const float *data, MovitPixelFormat pixel_format, Colorspace color_space, GammaCurve gamma_curve);
	Input *add_input(const unsigned char *data, MovitPixelFormat pixel_format, Colorspace color_space, GammaCurve gamma_curve);
	void run(float *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve);
	void run(unsigned char *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve);

private:
	void finalize_chain(Colorspace color_space, GammaCurve gamma_curve);

	EffectChain chain;
	GLuint fbo, texnum;
	unsigned width, height;
	bool finalized;
};

void expect_equal(const float *ref, const float *result, unsigned width, unsigned height, float largest_difference_limit = 1.5 / 255.0, float rms_limit = 0.2 / 255.0);
void expect_equal(const unsigned char *ref, const unsigned char *result, unsigned width, unsigned height, unsigned largest_difference_limit = 1, float rms_limit = 0.2);

#endif  // !defined(_TEST_UTIL_H)
