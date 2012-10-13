#include "test_util.h"
#include "flat_input.h"
#include "gtest/gtest.h"

#include <stdio.h>
#include <math.h>

#include <algorithm>

EffectChainTester::EffectChainTester(const float *data, unsigned width, unsigned height, MovitPixelFormat pixel_format, ColorSpace color_space, GammaCurve gamma_curve)
	: chain(width, height), width(width), height(height)
{
	add_input(data, pixel_format, color_space, gamma_curve);

	glGenTextures(1, &texnum);
	check_error();
	glBindTexture(GL_TEXTURE_2D, texnum);
	check_error();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
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

Input *EffectChainTester::add_input(const float *data, MovitPixelFormat pixel_format, ColorSpace color_space, GammaCurve gamma_curve)
{
	ImageFormat format;
	format.color_space = color_space;
	format.gamma_curve = gamma_curve;

	FlatInput *input = new FlatInput(format, pixel_format, GL_FLOAT, width, height);
	input->set_pixel_data(data);
	chain.add_input(input);
	return input;
}

void EffectChainTester::run(float *out_data, GLenum format, ColorSpace color_space, GammaCurve gamma_curve)
{
	ImageFormat image_format;
	image_format.color_space = color_space;
	image_format.gamma_curve = gamma_curve;
	chain.add_output(image_format);
	chain.finalize();

	chain.render_to_fbo(fbo, width, height);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glReadPixels(0, 0, width, height, format, GL_FLOAT, out_data);

	// Flip upside-down to compensate for different origin.
	for (unsigned y = 0; y < height / 2; ++y) {
		unsigned flip_y = height - y - 1;
		for (unsigned x = 0; x < width; ++x) {
			std::swap(out_data[y * width + x], out_data[flip_y * width + x]);
		}
	}
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
