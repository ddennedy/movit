#include "test_util.h"
#include "flat_input.h"
#include "gtest/gtest.h"

#include <stdio.h>
#include <math.h>

#include <algorithm>

EffectChainTester::EffectChainTester(const float *data, unsigned width, unsigned height, ColorSpace color_space, GammaCurve gamma_curve)
	: chain(width, height), width(width), height(height)
{
	add_input(data, color_space, gamma_curve);
}

Input *EffectChainTester::add_input(const float *data, ColorSpace color_space, GammaCurve gamma_curve)
{
	ImageFormat format;
	format.color_space = color_space;
	format.gamma_curve = gamma_curve;

	FlatInput *input = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, width, height);
	input->set_pixel_data(data);
	chain.add_input(input);
	return input;
}

void EffectChainTester::run(float *out_data, ColorSpace color_space, GammaCurve gamma_curve)
{
	ImageFormat format;
	format.color_space = color_space;
	format.gamma_curve = gamma_curve;
	chain.add_output(format);
	chain.finalize();

	glViewport(0, 0, width, height);
	chain.render_to_screen();

	glReadPixels(0, 0, width, height, GL_RED, GL_FLOAT, out_data);

	// Flip upside-down to compensate for different origin.
	for (unsigned y = 0; y < height / 2; ++y) {
		unsigned flip_y = height - y - 1;
		for (unsigned x = 0; x < width; ++x) {
			std::swap(out_data[y * width + x], out_data[flip_y * width + x]);
		}
	}
}

void expect_equal(const float *ref, const float *result, unsigned width, unsigned height)
{
	float largest_difference = -1.0f;
	float squared_difference = 0.0f;

	for (unsigned y = 0; y < height; ++y) {
		for (unsigned x = 0; x < width; ++x) {
			float diff = ref[y * width + x] - result[y * width + x];
			largest_difference = std::max(largest_difference, fabsf(diff));
			squared_difference += diff * diff;
		}
	}

	const float largest_difference_limit = 1.5 / 255.0;
	const float rms_limit = 0.5 / 255.0;

	EXPECT_LT(largest_difference, largest_difference_limit);

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
