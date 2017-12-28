#ifndef _MOVIT_TEST_UTIL_H
#define _MOVIT_TEST_UTIL_H 1

#include <epoxy/gl.h>
#ifdef HAVE_BENCHMARK
#include <benchmark/benchmark.h>
#endif
#include "effect_chain.h"
#include "fp16.h"
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
	Input *add_input(const fp16_int_t *data, MovitPixelFormat pixel_format, Colorspace color_space, GammaCurve gamma_curve, int input_width = -1, int input_height = -1);
	Input *add_input(const unsigned char *data, MovitPixelFormat pixel_format, Colorspace color_space, GammaCurve gamma_curve, int input_width = -1, int input_height = -1);

	void run(float *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void run(const std::vector<float *> &out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void run(unsigned char *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void run(const std::vector<unsigned char *> &out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void run(uint16_t *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void run_10_10_10_2(uint32_t *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);

#ifdef HAVE_BENCHMARK
	void benchmark(benchmark::State &state, float *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void benchmark(benchmark::State &state, const std::vector<float *> &out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void benchmark(benchmark::State &state, fp16_int_t *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void benchmark(benchmark::State &state, const std::vector<fp16_int_t *> &out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void benchmark(benchmark::State &state, unsigned char *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void benchmark(benchmark::State &state, const std::vector<unsigned char *> &out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void benchmark(benchmark::State &state, uint16_t *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	void benchmark_10_10_10_2(benchmark::State &state, uint32_t *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
#endif

	void add_output(const ImageFormat &format, OutputAlphaFormat alpha_format);
	void add_ycbcr_output(const ImageFormat &format, OutputAlphaFormat alpha_format, const YCbCrFormat &ycbcr_format, YCbCrOutputSplitting output_splitting = YCBCR_OUTPUT_INTERLEAVED, GLenum output_type = GL_UNSIGNED_BYTE);

private:
	void finalize_chain(Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format);

	template<class T>
	void internal_run(const std::vector<T *> &out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format = OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED
#ifdef HAVE_BENCHMARK
		, benchmark::State *state = nullptr
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

// A RAII class to pretend temporarily that we don't support compute shaders
// even if we do. Useful for testing or benchmarking the fragment shader path
// also on systems that support compute shaders.
class DisableComputeShadersTemporarily
{
public:
	// If disable_compute_shaders is false, this class effectively does nothing.
	// Otherwise, sets movit_compute_shaders_supported unconditionally to false.
	DisableComputeShadersTemporarily(bool disable_compute_shaders);

	// Restore the old value of movit_compute_shaders_supported.
	~DisableComputeShadersTemporarily();

	// Whether the current test should be skipped due to lack of compute shaders
	// (ie., disable_compute_shaders was _false_, but the system does not support
	// compute shaders). Will also output a message to stderr if so.
	bool should_skip();

#ifdef HAVE_BENCHMARK
	// Same, but outputs a message to the benchmark instead of to stderr.	
	bool should_skip(benchmark::State *benchmark_state);
#endif

	bool active() const { return disable_compute_shaders; }

private:
	bool disable_compute_shaders, saved_compute_shaders_supported;
};

}  // namespace movit

#endif  // !defined(_MOVIT_TEST_UTIL_H)
