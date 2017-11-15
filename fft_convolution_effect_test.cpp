// Unit tests for FFTConvolutionEffect.

#include <epoxy/gl.h>
#include <math.h>

#include "effect_chain.h"
#include "gtest/gtest.h"
#include "image_format.h"
#include "test_util.h"
#include "fft_convolution_effect.h"

namespace movit {

TEST(FFTConvolutionEffectTest, Identity) {
	const int size = 4;

	float data[size * size] = {
		0.1, 1.1, 2.1, 3.1,
		0.2, 1.2, 2.2, 3.2,
		0.3, 1.3, 2.3, 3.3,
		0.4, 1.4, 2.4, 3.4,
	};
	float out_data[size * size];

	for (int convolve_size = 1; convolve_size < 10; ++convolve_size) {
		float kernel[convolve_size * convolve_size];
		for (int i = 0; i < convolve_size * convolve_size; ++i) {
			kernel[i] = 0.0f;
		}
		kernel[0] = 1.0f;

		EffectChainTester tester(nullptr, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
		tester.add_input(data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, size, size);

		FFTConvolutionEffect *fft_effect = new FFTConvolutionEffect(size, size, convolve_size, convolve_size);
		tester.get_chain()->add_effect(fft_effect);
		fft_effect->set_convolution_kernel(kernel);
		tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

		expect_equal(data, out_data, size, size, 0.02, 0.003);
	}
}

TEST(FFTConvolutionEffectTest, Constant) {
	const int size = 4, convolve_size = 17;
	const float f = 7.0f;

	float data[size * size] = {
		0.1, 1.1, 2.1, 3.1,
		0.2, 1.2, 2.2, 3.2,
		0.3, 1.3, 2.3, 3.3,
		0.4, 1.4, 2.4, 3.4,
	};
	float expected_data[size * size] = {
		f * 0.1f, f * 1.1f, f * 2.1f, f * 3.1f,
		f * 0.2f, f * 1.2f, f * 2.2f, f * 3.2f,
		f * 0.3f, f * 1.3f, f * 2.3f, f * 3.3f,
		f * 0.4f, f * 1.4f, f * 2.4f, f * 3.4f,
	};
	float out_data[size * size];
	float kernel[convolve_size * convolve_size];
	for (int i = 0; i < convolve_size * convolve_size; ++i) {
		kernel[i] = 0.0f;
	}
	kernel[0] = f;

	EffectChainTester tester(nullptr, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.add_input(data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, size, size);

	FFTConvolutionEffect *fft_effect = new FFTConvolutionEffect(size, size, convolve_size, convolve_size);
	tester.get_chain()->add_effect(fft_effect);
	fft_effect->set_convolution_kernel(kernel);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	// Somewhat looser bounds due to the higher magnitude.
	expect_equal(expected_data, out_data, size, size, f * 0.03, f * 0.004);
}

TEST(FFTConvolutionEffectTest, MoveRight) {
	const int size = 4, convolve_size = 3;

	float data[size * size] = {
		0.1, 1.1, 2.1, 3.1,
		0.2, 1.2, 2.2, 3.2,
		0.3, 1.3, 2.3, 3.3,
		0.4, 1.4, 2.4, 3.4,
	};
	float kernel[convolve_size * convolve_size] = {
		0.0, 1.0, 0.0,
		0.0, 0.0, 0.0,
		0.0, 0.0, 0.0,
	};
	float expected_data[size * size] = {
		0.1, 0.1, 1.1, 2.1,
		0.2, 0.2, 1.2, 2.2,
		0.3, 0.3, 1.3, 2.3,
		0.4, 0.4, 1.4, 2.4,
	};
	float out_data[size * size];

	EffectChainTester tester(nullptr, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.add_input(data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, size, size);

	FFTConvolutionEffect *fft_effect = new FFTConvolutionEffect(size, size, convolve_size, convolve_size);
	tester.get_chain()->add_effect(fft_effect);
	fft_effect->set_convolution_kernel(kernel);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	expect_equal(expected_data, out_data, size, size, 0.02, 0.003);
}

TEST(FFTConvolutionEffectTest, MoveDown) {
	const int size = 4, convolve_size = 3;

	float data[size * size] = {
		0.1, 1.1, 2.1, 3.1,
		0.2, 1.2, 2.2, 3.2,
		0.3, 1.3, 2.3, 3.3,
		0.4, 1.4, 2.4, 3.4,
	};
	float kernel[convolve_size * convolve_size] = {
		0.0, 0.0, 0.0,
		1.0, 0.0, 0.0,
		0.0, 0.0, 0.0,
	};
	float expected_data[size * size] = {
		0.1, 1.1, 2.1, 3.1,
		0.1, 1.1, 2.1, 3.1,
		0.2, 1.2, 2.2, 3.2,
		0.3, 1.3, 2.3, 3.3,
	};
	float out_data[size * size];

	EffectChainTester tester(nullptr, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.add_input(data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, size, size);

	FFTConvolutionEffect *fft_effect = new FFTConvolutionEffect(size, size, convolve_size, convolve_size);
	tester.get_chain()->add_effect(fft_effect);
	fft_effect->set_convolution_kernel(kernel);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	expect_equal(expected_data, out_data, size, size, 0.02, 0.003);
}

TEST(FFTConvolutionEffectTest, MergeWithLeft) {
	const int size = 4, convolve_size = 3;

	float data[size * size] = {
		0.1, 1.1, 2.1, 3.1,
		0.2, 1.2, 2.2, 3.2,
		0.3, 1.3, 2.3, 3.3,
		0.4, 1.4, 2.4, 3.4,
	};
	float kernel[convolve_size * convolve_size] = {
		1.0, 1.0, 0.0,
		0.0, 0.0, 0.0,
		0.0, 0.0, 0.0,
	};
	float expected_data[size * size] = {
		0.1 + 0.1,  0.1 + 1.1,  1.1 + 2.1,  2.1 + 3.1,
		0.2 + 0.2,  0.2 + 1.2,  1.2 + 2.2,  2.2 + 3.2,
		0.3 + 0.3,  0.3 + 1.3,  1.3 + 2.3,  2.3 + 3.3,
		0.4 + 0.4,  0.4 + 1.4,  1.4 + 2.4,  2.4 + 3.4,
	};
	float out_data[size * size];

	EffectChainTester tester(nullptr, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.add_input(data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, size, size);

	FFTConvolutionEffect *fft_effect = new FFTConvolutionEffect(size, size, convolve_size, convolve_size);
	tester.get_chain()->add_effect(fft_effect);
	fft_effect->set_convolution_kernel(kernel);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	expect_equal(expected_data, out_data, size, size, 0.02, 0.003);
}

TEST(FFTConvolutionEffectTest, NegativeCoefficients) {
	const int size = 4;
	const int convolve_width = 3, convolve_height = 2;

	float data[size * size] = {
		0.1, 1.1, 2.1, 3.1,
		0.2, 1.2, 2.2, 3.2,
		0.3, 1.3, 2.3, 3.3,
		0.4, 1.4, 2.4, 3.4,
	};
	float kernel[convolve_width * convolve_height] = {
		1.0, 0.0,  0.0,
		0.0, 0.0, -0.5,
	};
	float expected_data[size * size] = {
		0.1 - 0.5 * 0.1,  1.1 - 0.5 * 0.1,  2.1 - 0.5 * 0.1,  3.1 - 0.5 * 1.1,
		0.2 - 0.5 * 0.1,  1.2 - 0.5 * 0.1,  2.2 - 0.5 * 0.1,  3.2 - 0.5 * 1.1,
		0.3 - 0.5 * 0.2,  1.3 - 0.5 * 0.2,  2.3 - 0.5 * 0.2,  3.3 - 0.5 * 1.2,
		0.4 - 0.5 * 0.3,  1.4 - 0.5 * 0.3,  2.4 - 0.5 * 0.3,  3.4 - 0.5 * 1.3,
	};
	float out_data[size * size];

	EffectChainTester tester(nullptr, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.add_input(data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, size, size);

	FFTConvolutionEffect *fft_effect = new FFTConvolutionEffect(size, size, convolve_width, convolve_height);
	tester.get_chain()->add_effect(fft_effect);
	fft_effect->set_convolution_kernel(kernel);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	expect_equal(expected_data, out_data, size, size, 0.02, 0.003);
}

}  // namespace movit
