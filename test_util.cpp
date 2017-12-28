#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <algorithm>
#include <epoxy/gl.h>
#include <gtest/gtest.h>
#include <gtest/gtest-message.h>

#include "flat_input.h"
#include "init.h"
#include "resource_pool.h"
#include "test_util.h"
#include "util.h"

using namespace std;

namespace movit {

class Input;

namespace {

// Not thread-safe, but this isn't a big problem for testing.
ResourcePool *get_static_pool()
{
	static ResourcePool *resource_pool = nullptr;
	if (!resource_pool) {
		resource_pool = new ResourcePool();
	}
	return resource_pool;
}

// Flip upside-down to compensate for different origin.
template<class T>
void vertical_flip(T *data, unsigned width, unsigned height)
{
	for (unsigned y = 0; y < height / 2; ++y) {
		unsigned flip_y = height - y - 1;
		for (unsigned x = 0; x < width; ++x) {
			swap(data[y * width + x], data[flip_y * width + x]);
		}
	}
}

void init_movit_for_test()
{
       CHECK(init_movit(".", MOVIT_DEBUG_OFF));
}

}  // namespace

EffectChainTester::EffectChainTester(const float *data, unsigned width, unsigned height,
                                     MovitPixelFormat pixel_format, Colorspace color_space, GammaCurve gamma_curve,
                                     GLenum framebuffer_format)
	: chain(width, height, get_static_pool()),
	  width(width),
	  height(height),
	  framebuffer_format(framebuffer_format),
	  output_added(false),
	  finalized(false)
{
	init_movit_for_test();

	if (data != nullptr) {
		add_input(data, pixel_format, color_space, gamma_curve);
	}
}

EffectChainTester::~EffectChainTester()
{
}

Input *EffectChainTester::add_input(const float *data, MovitPixelFormat pixel_format, Colorspace color_space, GammaCurve gamma_curve, int input_width, int input_height)
{
	ImageFormat format;
	format.color_space = color_space;
	format.gamma_curve = gamma_curve;

	if (input_width == -1) {
		input_width = width;
	}
	if (input_height == -1) {
		input_height = height;
	}

	FlatInput *input = new FlatInput(format, pixel_format, GL_FLOAT, input_width, input_height);
	input->set_pixel_data(data);
	chain.add_input(input);
	return input;
}

Input *EffectChainTester::add_input(const fp16_int_t *data, MovitPixelFormat pixel_format, Colorspace color_space, GammaCurve gamma_curve, int input_width, int input_height)
{
	ImageFormat format;
	format.color_space = color_space;
	format.gamma_curve = gamma_curve;

	if (input_width == -1) {
		input_width = width;
	}
	if (input_height == -1) {
		input_height = height;
	}

	FlatInput *input = new FlatInput(format, pixel_format, GL_HALF_FLOAT, input_width, input_height);
	input->set_pixel_data_fp16(data);
	chain.add_input(input);
	return input;
}

Input *EffectChainTester::add_input(const unsigned char *data, MovitPixelFormat pixel_format, Colorspace color_space, GammaCurve gamma_curve, int input_width, int input_height)
{
	ImageFormat format;
	format.color_space = color_space;
	format.gamma_curve = gamma_curve;

	if (input_width == -1) {
		input_width = width;
	}
	if (input_height == -1) {
		input_height = height;
	}

	FlatInput *input = new FlatInput(format, pixel_format, GL_UNSIGNED_BYTE, input_width, input_height);
	input->set_pixel_data(data);
	chain.add_input(input);
	return input;
}

void EffectChainTester::run(float *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format)
{
	internal_run<float>({out_data}, format, color_space, gamma_curve, alpha_format);
}

void EffectChainTester::run(const std::vector<float *> &out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format)
{
	internal_run<float>(out_data, format, color_space, gamma_curve, alpha_format);
}

void EffectChainTester::run(unsigned char *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format)
{
	internal_run<unsigned char>({out_data}, format, color_space, gamma_curve, alpha_format);
}

void EffectChainTester::run(const std::vector<unsigned char *> &out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format)
{
	internal_run<unsigned char>(out_data, format, color_space, gamma_curve, alpha_format);
}

void EffectChainTester::run(uint16_t *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format)
{
	internal_run<uint16_t>({out_data}, format, color_space, gamma_curve, alpha_format);
}

void EffectChainTester::run_10_10_10_2(uint32_t *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format)
{
	internal_run<uint32_t>({out_data}, format, color_space, gamma_curve, alpha_format);
}

#ifdef HAVE_BENCHMARK

void EffectChainTester::benchmark(benchmark::State &state, float *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format)
{
	internal_run<float>({out_data}, format, color_space, gamma_curve, alpha_format, &state);
}

void EffectChainTester::benchmark(benchmark::State &state, const std::vector<float *> &out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format)
{
	internal_run<float>(out_data, format, color_space, gamma_curve, alpha_format, &state);
}

void EffectChainTester::benchmark(benchmark::State &state, fp16_int_t *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format)
{
	internal_run<fp16_int_t>({out_data}, format, color_space, gamma_curve, alpha_format, &state);
}

void EffectChainTester::benchmark(benchmark::State &state, const std::vector<fp16_int_t *> &out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format)
{
	internal_run<fp16_int_t>(out_data, format, color_space, gamma_curve, alpha_format, &state);
}

void EffectChainTester::benchmark(benchmark::State &state, unsigned char *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format)
{
	internal_run<unsigned char>({out_data}, format, color_space, gamma_curve, alpha_format, &state);
}

void EffectChainTester::benchmark(benchmark::State &state, const std::vector<unsigned char *> &out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format)
{
	internal_run<unsigned char>(out_data, format, color_space, gamma_curve, alpha_format, &state);
}

void EffectChainTester::benchmark(benchmark::State &state, uint16_t *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format)
{
	internal_run<uint16_t>({out_data}, format, color_space, gamma_curve, alpha_format, &state);
}

void EffectChainTester::benchmark_10_10_10_2(benchmark::State &state, uint32_t *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format)
{
	internal_run<uint32_t>({out_data}, format, color_space, gamma_curve, alpha_format, &state);
}

#endif

template<class T>
void EffectChainTester::internal_run(const std::vector<T *> &out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format
#ifdef HAVE_BENCHMARK
, benchmark::State *benchmark_state
#endif
)
{
	if (!finalized) {
		finalize_chain(color_space, gamma_curve, alpha_format);
	}

	GLuint type;
	if (framebuffer_format == GL_RGBA8) {
		type = GL_UNSIGNED_BYTE;
	} else if (framebuffer_format == GL_RGBA16) {
		type = GL_UNSIGNED_SHORT;
	} else if (framebuffer_format == GL_RGBA16F && sizeof(T) == 2) {
		type = GL_HALF_FLOAT;
	} else if (framebuffer_format == GL_RGBA16F || framebuffer_format == GL_RGBA32F) {
		type = GL_FLOAT;
	} else if (framebuffer_format == GL_RGB10_A2) {
		type = GL_UNSIGNED_INT_2_10_10_10_REV;
	} else {
		// Add more here as needed.
		assert(false);
	}

	glActiveTexture(GL_TEXTURE0);
	check_error();

	vector<EffectChain::DestinationTexture> textures;
	for (unsigned i = 0; i < out_data.size(); ++i) {
		GLuint texnum = chain.get_resource_pool()->create_2d_texture(framebuffer_format, width, height);
		textures.push_back(EffectChain::DestinationTexture{texnum, framebuffer_format});

		// The output texture needs to have valid state to be written to by a compute shader.
		glBindTexture(GL_TEXTURE_2D, texnum);
		check_error();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		check_error();
	}

	chain.render_to_texture(textures, width, height);

#ifdef HAVE_BENCHMARK
	// If running benchmarks: Now we've warmed up everything, so let's run the
	// actual benchmark loop.
	if (benchmark_state != nullptr) {
		glFinish();
	        size_t iters = benchmark_state->max_iterations;
		for (auto _ : *benchmark_state) {
			chain.render_to_texture(textures, width, height);
			if (--iters == 0) {
				glFinish();
			}
		}
		benchmark_state->SetItemsProcessed(benchmark_state->iterations() * width * height);
	}
#endif

	for (unsigned i = 0; i < out_data.size(); ++i) {
		T *ptr = out_data[i];
		glBindTexture(GL_TEXTURE_2D, textures[i].texnum);
		check_error();
		if (!epoxy_is_desktop_gl() && (format == GL_RED || format == GL_BLUE || format == GL_ALPHA)) {
			// GLES will only read GL_RGBA.
			std::unique_ptr<T[]> temp(new T[width * height * 4]);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, type, temp.get());
			check_error();
			if (format == GL_ALPHA) {
				for (unsigned j = 0; j < width * height; ++j) {
					ptr[j] = temp[j * 4 + 3];
				}
			} else if (format == GL_BLUE) {
				for (unsigned j = 0; j < width * height; ++j) {
					ptr[j] = temp[j * 4 + 2];
				}
			} else {
				for (unsigned j = 0; j < width * height; ++j) {
					ptr[j] = temp[j * 4];
				}
			}
		} else {
			glGetTexImage(GL_TEXTURE_2D, 0, format, type, ptr);
			check_error();
		}

		if (format == GL_RGBA && (type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_SHORT || type == GL_FLOAT)) {
			vertical_flip(ptr, width * 4, height);
		} else {
			vertical_flip(ptr, width, height);
		}
	}

	for (unsigned i = 0; i < out_data.size(); ++i) {
		chain.get_resource_pool()->release_2d_texture(textures[i].texnum);
	}
}

void EffectChainTester::add_output(const ImageFormat &format, OutputAlphaFormat alpha_format)
{
	chain.add_output(format, alpha_format);
	output_added = true;
}

void EffectChainTester::add_ycbcr_output(const ImageFormat &format, OutputAlphaFormat alpha_format, const YCbCrFormat &ycbcr_format, YCbCrOutputSplitting output_splitting, GLenum type)
{
	chain.add_ycbcr_output(format, alpha_format, ycbcr_format, output_splitting, type);
	output_added = true;
}

void EffectChainTester::finalize_chain(Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format)
{
	assert(!finalized);
	if (!output_added) {
		ImageFormat image_format;
		image_format.color_space = color_space;
		image_format.gamma_curve = gamma_curve;
		chain.add_output(image_format, alpha_format);
		output_added = true;
	}
	chain.finalize();
	finalized = true;
}

void expect_equal(const float *ref, const float *result, unsigned width, unsigned height, float largest_difference_limit, float rms_limit)
{
	float largest_difference = -1.0f;
	float squared_difference = 0.0f;
	int largest_diff_x = -1, largest_diff_y = -1;

	for (unsigned y = 0; y < height; ++y) {
		for (unsigned x = 0; x < width; ++x) {
			float diff = ref[y * width + x] - result[y * width + x];
			if (fabs(diff) > largest_difference) {
				largest_difference = fabs(diff);
				largest_diff_x = x;
				largest_diff_y = y;
			}
			squared_difference += diff * diff;
		}
	}

	EXPECT_LT(largest_difference, largest_difference_limit)
		<< "Largest difference is in x=" << largest_diff_x << ", y=" << largest_diff_y << ":\n"
		<< "Reference: " << ref[largest_diff_y * width + largest_diff_x] << "\n"
		<< "Result:    " << result[largest_diff_y * width + largest_diff_x];

	float rms = sqrt(squared_difference) / (width * height);
	EXPECT_LT(rms, rms_limit);

	if (largest_difference >= largest_difference_limit || isnan(rms) || rms >= rms_limit) {
		fprintf(stderr, "Dumping matrices for easier debugging, since at least one test failed.\n");

		fprintf(stderr, "Reference:\n");
		for (unsigned y = 0; y < height; ++y) {
			for (unsigned x = 0; x < width; ++x) {
				fprintf(stderr, "%7.4f ", ref[y * width + x]);
			}
			fprintf(stderr, "\n");
		}

		fprintf(stderr, "\nResult:\n");
		for (unsigned y = 0; y < height; ++y) {
			for (unsigned x = 0; x < width; ++x) {
				fprintf(stderr, "%7.4f ", result[y * width + x]);
			}
			fprintf(stderr, "\n");
		}
	}
}

void expect_equal(const unsigned char *ref, const unsigned char *result, unsigned width, unsigned height, unsigned largest_difference_limit, float rms_limit)
{
	assert(width > 0);
	assert(height > 0);

	float *ref_float = new float[width * height];
	float *result_float = new float[width * height];

	for (unsigned y = 0; y < height; ++y) {
		for (unsigned x = 0; x < width; ++x) {
			ref_float[y * width + x] = ref[y * width + x];
			result_float[y * width + x] = result[y * width + x];
		}
	}

	expect_equal(ref_float, result_float, width, height, largest_difference_limit, rms_limit);

	delete[] ref_float;
	delete[] result_float;
}

void expect_equal(const uint16_t *ref, const uint16_t *result, unsigned width, unsigned height, unsigned largest_difference_limit, float rms_limit)
{
	assert(width > 0);
	assert(height > 0);

	float *ref_float = new float[width * height];
	float *result_float = new float[width * height];

	for (unsigned y = 0; y < height; ++y) {
		for (unsigned x = 0; x < width; ++x) {
			ref_float[y * width + x] = ref[y * width + x];
			result_float[y * width + x] = result[y * width + x];
		}
	}

	expect_equal(ref_float, result_float, width, height, largest_difference_limit, rms_limit);

	delete[] ref_float;
	delete[] result_float;
}

void expect_equal(const int *ref, const int *result, unsigned width, unsigned height, unsigned largest_difference_limit, float rms_limit)
{
	assert(width > 0);
	assert(height > 0);

	float *ref_float = new float[width * height];
	float *result_float = new float[width * height];

	for (unsigned y = 0; y < height; ++y) {
		for (unsigned x = 0; x < width; ++x) {
			ref_float[y * width + x] = ref[y * width + x];
			result_float[y * width + x] = result[y * width + x];
		}
	}

	expect_equal(ref_float, result_float, width, height, largest_difference_limit, rms_limit);

	delete[] ref_float;
	delete[] result_float;
}

void test_accuracy(const float *expected, const float *result, unsigned num_values, double absolute_error_limit, double relative_error_limit, double local_relative_error_limit, double rms_limit)
{
	double squared_difference = 0.0;
	for (unsigned i = 0; i < num_values; ++i) {
		double absolute_error = fabs(expected[i] - result[i]);
		squared_difference += absolute_error * absolute_error;
		EXPECT_LT(absolute_error, absolute_error_limit);

		if (expected[i] > 0.0) {
			double relative_error = fabs(absolute_error / expected[i]);

			EXPECT_LT(relative_error, relative_error_limit);
		}
		if (i < num_values - 1) {
			double delta = expected[i + 1] - expected[i];
			double local_relative_error = fabs(absolute_error / delta);
			EXPECT_LT(local_relative_error, local_relative_error_limit);
		}
	}
	double rms = sqrt(squared_difference) / num_values;
	EXPECT_LT(rms, rms_limit);
}

double srgb_to_linear(double x)
{
	// From the Wikipedia article on sRGB.
	if (x < 0.04045) {
		return x / 12.92;
	} else {
		return pow((x + 0.055) / 1.055, 2.4);
	}
}

double linear_to_srgb(double x)
{
	// From the Wikipedia article on sRGB.
	if (x < 0.0031308) {
		return 12.92 * x;
	} else {
		return 1.055 * pow(x, 1.0 / 2.4) - 0.055;
	}
}

DisableComputeShadersTemporarily::DisableComputeShadersTemporarily(bool disable_compute_shaders)
	: disable_compute_shaders(disable_compute_shaders)
{
	init_movit_for_test();
	saved_compute_shaders_supported = movit_compute_shaders_supported;
	if (disable_compute_shaders) {
		movit_compute_shaders_supported = false;
	}
}

DisableComputeShadersTemporarily::~DisableComputeShadersTemporarily()
{
	movit_compute_shaders_supported = saved_compute_shaders_supported;
}

bool DisableComputeShadersTemporarily::should_skip()
{
	if (disable_compute_shaders) {
		return false;
	}

	if (!movit_compute_shaders_supported) {
		fprintf(stderr, "Compute shaders not supported; skipping.\n");
		return true;
	}
	return false;
}

#ifdef HAVE_BENCHMARK
bool DisableComputeShadersTemporarily::should_skip(benchmark::State *benchmark_state)
{
	if (disable_compute_shaders) {
		return false;
	}

	if (!movit_compute_shaders_supported) {
		benchmark_state->SkipWithError("Compute shaders not supported");
		return true;
	}
	return false;
}
#endif

}  // namespace movit
