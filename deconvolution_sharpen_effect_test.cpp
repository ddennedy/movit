// Unit tests for DeconvolutionSharpenEffect.

#include "test_util.h"
#include "gtest/gtest.h"
#include "deconvolution_sharpen_effect.h"

TEST(DeconvolutionSharpenEffect, DeconvolvesCircularBlur) {
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

TEST(DeconvolutionSharpenEffect, DeconvolvesGaussianBlur) {
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

// TODO: Test no-op (both radii equal to zero).
// TODO: Test correlation and noise parameters.
