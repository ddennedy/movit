#include "init.h"
#include "test_util.h"
#include "flat_input.h"
#include "gtest/gtest.h"

#include <stdio.h>
#include <math.h>

#include <algorithm>

namespace {

// Flip upside-down to compensate for different origin.
template<class T>
void vertical_flip(T *data, unsigned width, unsigned height)
{
	for (unsigned y = 0; y < height / 2; ++y) {
		unsigned flip_y = height - y - 1;
		for (unsigned x = 0; x < width; ++x) {
			std::swap(data[y * width + x], data[flip_y * width + x]);
		}
	}
}

}  // namespace

EffectChainTester::EffectChainTester(const float *data, unsigned width, unsigned height,
                                     MovitPixelFormat pixel_format, Colorspace color_space, GammaCurve gamma_curve,
                                     GLenum framebuffer_format)
	: chain(width, height), width(width), height(height), finalized(false)
{
	init_movit(".", MOVIT_DEBUG_ON);

	if (data != NULL) {
		add_input(data, pixel_format, color_space, gamma_curve);
	}

	glGenTextures(1, &texnum);
	check_error();
	glBindTexture(GL_TEXTURE_2D, texnum);
	check_error();
	glTexImage2D(GL_TEXTURE_2D, 0, framebuffer_format, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	check_error();

	glGenFramebuffers(1, &fbo);
	check_error();
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	check_error();
	glFramebufferTexture2D(
		GL_FRAMEBUFFER,
		GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D,
		texnum,
		0);
	check_error();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	check_error();
}

EffectChainTester::~EffectChainTester()
{
	glDeleteFramebuffers(1, &fbo);
	check_error();
	glDeleteTextures(1, &texnum);
	check_error();
}

Input *EffectChainTester::add_input(const float *data, MovitPixelFormat pixel_format, Colorspace color_space, GammaCurve gamma_curve)
{
	ImageFormat format;
	format.color_space = color_space;
	format.gamma_curve = gamma_curve;

	FlatInput *input = new FlatInput(format, pixel_format, GL_FLOAT, width, height);
	input->set_pixel_data(data);
	chain.add_input(input);
	return input;
}

Input *EffectChainTester::add_input(const unsigned char *data, MovitPixelFormat pixel_format, Colorspace color_space, GammaCurve gamma_curve)
{
	ImageFormat format;
	format.color_space = color_space;
	format.gamma_curve = gamma_curve;

	FlatInput *input = new FlatInput(format, pixel_format, GL_UNSIGNED_BYTE, width, height);
	input->set_pixel_data(data);
	chain.add_input(input);
	return input;
}

void EffectChainTester::run(float *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format)
{
	if (!finalized) {
		finalize_chain(color_space, gamma_curve, alpha_format);
	}

	chain.render_to_fbo(fbo, width, height);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glReadPixels(0, 0, width, height, format, GL_FLOAT, out_data);

	if (format == GL_RGBA) {
		width *= 4;
	}

	vertical_flip(out_data, width, height);
}

void EffectChainTester::run(unsigned char *out_data, GLenum format, Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format)
{
	if (!finalized) {
		finalize_chain(color_space, gamma_curve, alpha_format);
	}

	chain.render_to_fbo(fbo, width, height);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glReadPixels(0, 0, width, height, format, GL_UNSIGNED_BYTE, out_data);

	if (format == GL_RGBA) {
		width *= 4;
	}

	vertical_flip(out_data, width, height);
}

void EffectChainTester::finalize_chain(Colorspace color_space, GammaCurve gamma_curve, OutputAlphaFormat alpha_format)
{
	assert(!finalized);
	ImageFormat image_format;
	image_format.color_space = color_space;
	image_format.gamma_curve = gamma_curve;
	chain.add_output(image_format, alpha_format);
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

	if (largest_difference >= largest_difference_limit || rms >= rms_limit) {
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
	float *ref_float = new float[width * height];
	float *result_float = new float[width * height];

	for (unsigned y = 0; y < height; ++y) {
		for (unsigned x = 0; x < width; ++x) {
			ref_float[y * width + x] = ref[y * width + x];
			result_float[y * width + x] = result[y * width + x];
		}
	}

	expect_equal(ref_float, result_float, width, height, largest_difference_limit, rms_limit);
}
