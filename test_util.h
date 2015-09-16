#ifndef _MOVIT_TEST_UTIL_H
#define _MOVIT_TEST_UTIL_H 1

#include <epoxy/gl.h>
#include "effect_chain.h"
#include "image_format.h"

namespace movit {

class Input;

class EffectChainTester {
public:
	EffectChainTester(const float *data, unsigned width, unsigned height,
	                  MovitPixelFormat pixel_format = FORMAT_GRAYSCALE,
	                  Colorspace color_space = COLORSPACE_sRGB,
	                  GammaCurve gamma_curve = GAMMA_LINEAR,
	                  GLenum framebuffer_format = GL_RGBA16F_ARB);
	~EffectChainTester();
	
	EffectChain *get_chain() { return &chain; }
	Input *add_input(const float *data, MovitPixelFormat pixel_format, Colorspace color_space, GammaCurve gamma_curve, int input_width = -1, int input_height = -1);
	Input *add_input(const unsigned char *data, MovitPixelFormat pixel_format, Colorspace color_space, GammaCurve gamma_curve, int input_width = -1, int input_height = -1);
	void run(float *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void run(unsigned char *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void add_output(const ImageFormat &format, OutputAlphaFormat alpha_format);
	void add_ycbcr_output(const ImageFormat &format, OutputAlphaFormat alpha_format, const YCbCrFormat &ycbcr_format);

private:
	void finalize_chain(Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format);

	template<class T>
	void internal_run(T *out_data, GLenum internal_format, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);

	EffectChain chain;
	unsigned width, height;
	GLenum framebuffer_format;
	bool output_added;
	bool finalized;
};

void expect_equal(const float *ref, const float *result, unsigned width, unsigned height, float largest_difference_limit = 1.5 / 255.0, float rms_limit = 0.2 / 255.0);
void expect_equal(const unsigned char *ref, const unsigned char *result, unsigned width, unsigned height, unsigned largest_difference_limit = 1, float rms_limit = 0.2);
void test_accuracy(const float *expected, const float *result, unsigned num_values, double absolute_error_limit, double relative_error_limit, double local_relative_error_limit, double rms_limit);

}  // namespace movit

#endif  // !defined(_MOVIT_TEST_UTIL_H)
