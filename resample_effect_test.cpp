// Unit tests for ResampleEffect.

#include "test_util.h"
#include "gtest/gtest.h"
#include "resample_effect.h"
#include "flat_input.h"

namespace {

float sinc(float x)
{
	return sin(M_PI * x) / (M_PI * x);
}

float lanczos(float x, float a)
{
	if (fabs(x) >= a) {
		return 0.0f;
	} else {
		return sinc(x) * sinc(x / a);
	}
}

}  // namespace

TEST(ResampleEffectTest, IdentityTransformDoesNothing) {
	const int size = 4;

	float data[size * size] = {
		0.0, 1.0, 0.0, 1.0,
		0.0, 1.0, 1.0, 0.0,
		0.0, 0.5, 1.0, 0.5,
		0.0, 0.0, 0.0, 0.0,
	};
	float out_data[size * size];

	EffectChainTester tester(data, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *resample_effect = tester.get_chain()->add_effect(new ResampleEffect());
	ASSERT_TRUE(resample_effect->set_int("width", 4));
	ASSERT_TRUE(resample_effect->set_int("height", 4));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(data, out_data, size, size);
}

TEST(ResampleEffectTest, UpscaleByTwoGetsCorrectPixelCenters) {
	const int size = 5;

	float data[size * size] = {
		0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0,
	};
	float expected_data[size * size * 4], out_data[size * size * 4];

	for (int y = 0; y < size * 2; ++y) {
		for (int x = 0; x < size * 2; ++x) {
			float weight = lanczos((x - size + 0.5f) * 0.5f, 3.0f);
			weight *= lanczos((y - size + 0.5f) * 0.5f, 3.0f);
			expected_data[y * (size * 2) + x] = weight;
		}
	}

	EffectChainTester tester(NULL, size * 2, size * 2, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	FlatInput *input = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, size, size);
	input->set_pixel_data(data);
	tester.get_chain()->add_input(input);

	Effect *resample_effect = tester.get_chain()->add_effect(new ResampleEffect());
	ASSERT_TRUE(resample_effect->set_int("width", size * 2));
	ASSERT_TRUE(resample_effect->set_int("height", size * 2));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, size * 2, size * 2);
}

TEST(ResampleEffectTest, DownscaleByTwoGetsCorrectPixelCenters) {
	const int size = 5;

	// This isn't a perfect dot, since the Lanczos filter has a slight
	// sharpening effect; the most important thing is that we have kept
	// the texel center right (everything is nicely symmetric).
	// The approximate magnitudes have been checked against ImageMagick.
	float expected_data[size * size] = {
		 0.0045, -0.0067, -0.0598, -0.0067,  0.0045, 
		-0.0067,  0.0099,  0.0886,  0.0099, -0.0067, 
		-0.0598,  0.0886,  0.7930,  0.0886, -0.0598, 
		-0.0067,  0.0099,  0.0886,  0.0099, -0.0067, 
		 0.0045, -0.0067, -0.0598, -0.0067,  0.0045, 
	};
	float data[size * size * 4], out_data[size * size];

	for (int y = 0; y < size * 2; ++y) {
		for (int x = 0; x < size * 2; ++x) {
			float weight = lanczos((x - size + 0.5f) * 0.5f, 3.0f);
			weight *= lanczos((y - size + 0.5f) * 0.5f, 3.0f);
			data[y * (size * 2) + x] = weight;
		}
	}

	EffectChainTester tester(NULL, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	FlatInput *input = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, size * 2, size * 2);
	input->set_pixel_data(data);
	tester.get_chain()->add_input(input);

	Effect *resample_effect = tester.get_chain()->add_effect(new ResampleEffect());
	ASSERT_TRUE(resample_effect->set_int("width", size));
	ASSERT_TRUE(resample_effect->set_int("height", size));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, size, size);
}

TEST(ResampleEffectTest, UpscaleByThreeGetsCorrectPixelCenters) {
	const int size = 5;

	float data[size * size] = {
		0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0,
	};
	float out_data[size * size * 9];

	EffectChainTester tester(NULL, size * 3, size * 3, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	FlatInput *input = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, size, size);
	input->set_pixel_data(data);
	tester.get_chain()->add_input(input);

	Effect *resample_effect = tester.get_chain()->add_effect(new ResampleEffect());
	ASSERT_TRUE(resample_effect->set_int("width", size * 3));
	ASSERT_TRUE(resample_effect->set_int("height", size * 3));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	// We only bother checking that the middle pixel is still correct,
	// and that symmetry holds.
	EXPECT_FLOAT_EQ(1.0, out_data[7 * (size * 3) + 7]);
	for (unsigned y = 0; y < size * 3; ++y) {
		for (unsigned x = 0; x < size * 3; ++x) {
			EXPECT_FLOAT_EQ(out_data[y * (size * 3) + x], out_data[(size * 3 - y - 1) * (size * 3) + x]);
			EXPECT_FLOAT_EQ(out_data[y * (size * 3) + x], out_data[y * (size * 3) + (size * 3 - x - 1)]);
		}
	}
}
