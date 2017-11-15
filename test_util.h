#ifndef _MOVIT_TEST_UTIL_H
#define _MOVIT_TEST_UTIL_H 1

#include <epoxy/gl.h>
#ifdef HAVE_BENCHMARK
#include <benchmark/benchmark.h>
#endif
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
	void run(float *out_data, float *out_data2, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void run(float *out_data, float *out_data2, float *out_data3, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void run(float *out_data, float *out_data2, float *out_data3, float *out_data4, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void run(unsigned char *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void run(unsigned char *out_data, unsigned char *out_data2, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void run(unsigned char *out_data, unsigned char *out_data2, unsigned char *out_data3, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void run(unsigned char *out_data, unsigned char *out_data2, unsigned char *out_data3, unsigned char *out_data4, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void run(uint16_t *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void run_10_10_10_2(uint32_t *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);

#ifdef HAVE_BENCHMARK
	void benchmark(benchmark::State &state, float *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void benchmark(benchmark::State &state, float *out_data, float *out_data2, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void benchmark(benchmark::State &state, float *out_data, float *out_data2, float *out_data3, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void benchmark(benchmark::State &state, float *out_data, float *out_data2, float *out_data3, float *out_data4, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void benchmark(benchmark::State &state, unsigned char *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void benchmark(benchmark::State &state, unsigned char *out_data, unsigned char *out_data2, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void benchmark(benchmark::State &state, unsigned char *out_data, unsigned char *out_data2, unsigned char *out_data3, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void benchmark(benchmark::State &state, unsigned char *out_data, unsigned char *out_data2, unsigned char *out_data3, unsigned char *out_data4, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void benchmark(benchmark::State &state, uint16_t *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void benchmark_10_10_10_2(benchmark::State &state, uint32_t *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
#endif

	void add_output(const ImageFormat &format, OutputAlphaFormat alpha_format);
	void add_ycbcr_output(const ImageFormat &format, OutputAlphaFormat alpha_format, const YCbCrFormat &ycbcr_format, YCbCrOutputSplitting output_splitting = YCBCR_OUTPUT_INTERLEAVED, GLenum output_type = GL_UNSIGNED_BYTE);

private:
	void finalize_chain(Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format);

	template<class T>
	void internal_run(T *out_data, T *out_data2, T *out_data3, T *out_data4, GLenum internal_format, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED
#ifdef HAVE_BENCHMARK
		, benchmark::State *state = NULL
#endif
);

	EffectChain chain;
	unsigned width, height;
	GLenum framebuffer_format;
	bool output_added;
	bool finalized;
};

void expect_equal(const float *ref, const float *result, unsigned width, unsigned height, float largest_difference_limit = 1.5 / 255.0, float rms_limit = 0.2 / 255.0);
void expect_equal(const unsigned char *ref, const unsigned char *result, unsigned width, unsigned height, unsigned largest_difference_limit = 1, float rms_limit = 0.2);
void expect_equal(const uint16_t *ref, const uint16_t *result, unsigned width, unsigned height, unsigned largest_difference_limit = 1, float rms_limit = 0.2);
void expect_equal(const int *ref, const int *result, unsigned width, unsigned height, unsigned largest_difference_limit = 1, float rms_limit = 0.2);
void test_accuracy(const float *expected, const float *result, unsigned num_values, double absolute_error_limit, double relative_error_limit, double local_relative_error_limit, double rms_limit);

// Convert an sRGB encoded value (0.0 to 1.0, inclusive) to linear light.
// Undefined for values outside 0.0..1.0.
double srgb_to_linear(double x);

// Convert a value in linear light (0.0 to 1.0, inclusive) to sRGB.
// Undefined for values outside 0.0..1.0.
double linear_to_srgb(double x);

}  // namespace movit

#endif  // !defined(_MOVIT_TEST_UTIL_H)
