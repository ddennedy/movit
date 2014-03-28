// Unit tests for DeconvolutionSharpenEffect.

#include <epoxy/gl.h>
#include <math.h>
#include <stdlib.h>

#include "deconvolution_sharpen_effect.h"
#include "effect_chain.h"
#include "gtest/gtest.h"
#include "image_format.h"
#include "test_util.h"

namespace movit {

TEST(DeconvolutionSharpenEffectTest, IdentityTransformDoesNothing) {
	const int size = 4;

	float data[size * size] = {
		0.0, 1.0, 0.0, 1.0,
		0.0, 1.0, 1.0, 0.0,
		0.0, 0.5, 1.0, 0.5,
		0.0, 0.0, 0.0, 0.0,
	};
	float out_data[size * size];

	EffectChainTester tester(data, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *deconvolution_effect = tester.get_chain()->add_effect(new DeconvolutionSharpenEffect());
	ASSERT_TRUE(deconvolution_effect->set_int("matrix_size", 5));
	ASSERT_TRUE(deconvolution_effect->set_float("circle_radius", 0.0f));
	ASSERT_TRUE(deconvolution_effect->set_float("gaussian_radius", 0.0f));
	ASSERT_TRUE(deconvolution_effect->set_float("correlation", 0.0001f));
	ASSERT_TRUE(deconvolution_effect->set_float("noise", 0.0f));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(data, out_data, size, size);
}

TEST(DeconvolutionSharpenEffectTest, DeconvolvesCircularBlur) {
	const int size = 13;

	// Matches exactly a circular blur kernel with radius 2.0.
	float data[size * size] = {
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.017016, 0.038115, 0.017016, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.017016, 0.078381, 0.079577, 0.078381, 0.017016, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.038115, 0.079577, 0.079577, 0.079577, 0.038115, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.017016, 0.078381, 0.079577, 0.078381, 0.017016, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.017016, 0.038115, 0.017016, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
	};
	float expected_data[size * size] = {
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 1.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
	};
	float out_data[size * size];

	EffectChainTester tester(data, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *deconvolution_effect = tester.get_chain()->add_effect(new DeconvolutionSharpenEffect());
	ASSERT_TRUE(deconvolution_effect->set_int("matrix_size", 5));
	ASSERT_TRUE(deconvolution_effect->set_float("circle_radius", 2.0f));
	ASSERT_TRUE(deconvolution_effect->set_float("gaussian_radius", 0.0f));
	ASSERT_TRUE(deconvolution_effect->set_float("correlation", 0.0001f));
	ASSERT_TRUE(deconvolution_effect->set_float("noise", 0.0f));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	// The limits have to be quite lax; deconvolution is not an exact operation.
	expect_equal(expected_data, out_data, size, size, 0.15f, 0.005f);
}

TEST(DeconvolutionSharpenEffectTest, DeconvolvesGaussianBlur) {
	const int size = 13;
	const float sigma = 0.5f;

	float data[size * size], out_data[size * size];
	float expected_data[] = {
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 1.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
	};

	float sum = 0.0f;	
	for (int y = 0; y < size; ++y) {
		for (int x = 0; x < size; ++x) {
			float z = hypot(x - 6, y - 6);
			data[y * size + x] = exp(-z*z / (2.0 * sigma * sigma)) / (2.0 * M_PI * sigma * sigma);
			sum += data[y * size + x];
		}
	}
	for (int y = 0; y < size; ++y) {
		for (int x = 0; x < size; ++x) {
			data[y * size + x] /= sum;
		}
	}

	EffectChainTester tester(data, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *deconvolution_effect = tester.get_chain()->add_effect(new DeconvolutionSharpenEffect());
	ASSERT_TRUE(deconvolution_effect->set_int("matrix_size", 5));
	ASSERT_TRUE(deconvolution_effect->set_float("circle_radius", 0.0f));
	ASSERT_TRUE(deconvolution_effect->set_float("gaussian_radius", sigma));
	ASSERT_TRUE(deconvolution_effect->set_float("correlation", 0.0001f));
	ASSERT_TRUE(deconvolution_effect->set_float("noise", 0.0f));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	// We don't actually need to adjust the limits here; deconvolution of
	// this kernel is pretty much exact.
	expect_equal(expected_data, out_data, size, size);
}

TEST(DeconvolutionSharpenEffectTest, NoiseAndCorrelationControlsReduceNoiseBoosting) {
	const int size = 13;
	const float sigma = 0.5f;

	float data[size * size], out_data[size * size];
	float expected_data[size * size] = {
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 1.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.0, 0.0, 0.0, 0.0, 
	};

	// Gaussian kernel.
	float sum = 0.0f;	
	for (int y = 0; y < size; ++y) {
		for (int x = 0; x < size; ++x) {
			float z = hypot(x - 6, y - 6);
			data[y * size + x] = exp(-z*z / (2.0 * sigma * sigma)) / (2.0 * M_PI * sigma * sigma);
			sum += data[y * size + x];
		}
	}
	for (int y = 0; y < size; ++y) {
		for (int x = 0; x < size; ++x) {
			data[y * size + x] /= sum;
		}
	}

	// Corrupt with some uniform noise.
	srand(1234);
	for (int i = 0; i < size * size; ++i) {
		data[i] += 0.1 * ((float)rand() / RAND_MAX - 0.5);
	}

	EffectChainTester tester(data, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *deconvolution_effect = tester.get_chain()->add_effect(new DeconvolutionSharpenEffect());
	ASSERT_TRUE(deconvolution_effect->set_int("matrix_size", 5));
	ASSERT_TRUE(deconvolution_effect->set_float("circle_radius", 0.0f));
	ASSERT_TRUE(deconvolution_effect->set_float("gaussian_radius", 0.5f));
	ASSERT_TRUE(deconvolution_effect->set_float("correlation", 0.5f));
	ASSERT_TRUE(deconvolution_effect->set_float("noise", 0.1f));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	float sumsq_in = 0.0f, sumsq_out = 0.0f;
	for (int i = 0; i < size * size; ++i) {
		sumsq_in += data[i] * data[i];
		sumsq_out += out_data[i] * out_data[i];
	}

	// The limits have to be quite lax; deconvolution is not an exact operation.
	// We special-case the center sample since it's the one with the largest error
	// almost no matter what we do, so we don't want that to be the dominating
	// factor in the outlier tests.
	int center = size / 2;
	EXPECT_GT(out_data[center * size + center], 0.5f);
	out_data[center * size + center] = 1.0f;
	expect_equal(expected_data, out_data, size, size, 0.20f, 0.005f);

	// Check that we didn't boost total energy (which in this case means the noise) more than 10%.
	EXPECT_LT(sumsq_out, sumsq_in * 1.1f);
}

TEST(DeconvolutionSharpenEffectTest, CircularDeconvolutionKeepsAlpha) {
	// Somewhat bigger, to make sure we are much bigger than the matrix size.
	const int size = 32;

	float data[size * size * 4];
	float out_data[size * size];
	float expected_alpha[size * size];

	// Checkerbox pattern.
	for (int y = 0; y < size; ++y) {
		for (int x = 0; x < size; ++x) {
			int c = (y ^ x) & 1;
			data[(y * size + x) * 4 + 0] = c;
			data[(y * size + x) * 4 + 1] = c;
			data[(y * size + x) * 4 + 2] = c;
			data[(y * size + x) * 4 + 3] = 1.0;
			expected_alpha[y * size + x] = 1.0;
		}
	}

	EffectChainTester tester(data, size, size, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *deconvolution_effect = tester.get_chain()->add_effect(new DeconvolutionSharpenEffect());
	ASSERT_TRUE(deconvolution_effect->set_int("matrix_size", 5));
	ASSERT_TRUE(deconvolution_effect->set_float("circle_radius", 2.0f));
	ASSERT_TRUE(deconvolution_effect->set_float("gaussian_radius", 0.0f));
	ASSERT_TRUE(deconvolution_effect->set_float("correlation", 0.0001f));
	ASSERT_TRUE(deconvolution_effect->set_float("noise", 0.0f));
	tester.run(out_data, GL_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_alpha, out_data, size, size);
}

}  // namespace movit
