// Unit tests for BlurEffect.
#include <epoxy/gl.h>
#include <math.h>
#include <string.h>

#include "blur_effect.h"
#include "effect_chain.h"
#include "gtest/gtest.h"
#include "image_format.h"
#include "test_util.h"

namespace movit {

TEST(BlurEffectTest, IdentityTransformDoesNothing) {
	const int size = 4;

	float data[size * size] = {
		0.0, 1.0, 0.0, 1.0,
		0.0, 1.0, 1.0, 0.0,
		0.0, 0.5, 1.0, 0.5,
		0.0, 0.0, 0.0, 0.0,
	};
	float out_data[size * size];

	for (int num_taps = 2; num_taps < 20; num_taps += 2) {
		EffectChainTester tester(data, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
		Effect *blur_effect = tester.get_chain()->add_effect(new BlurEffect());
		ASSERT_TRUE(blur_effect->set_float("radius", 0.0f));
		ASSERT_TRUE(blur_effect->set_int("num_taps", num_taps));
		tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

		expect_equal(data, out_data, size, size);
	}
}

namespace {

void add_blurred_point(float *out, int size, int x0, int y0, float strength, float sigma)
{
	// From http://en.wikipedia.org/wiki/Logistic_distribution#Alternative_parameterization.
	const float c1 = M_PI / (sigma * 4 * sqrt(3.0f));
	const float c2 = M_PI / (sigma * 2.0 * sqrt(3.0f));

	for (int y = 0; y < size; ++y) {
		for (int x = 0; x < size; ++x) {
			float xd = c2 * (x - x0);
			float yd = c2 * (y - y0);
			out[y * size + x] += (strength * c1 * c1) / (cosh(xd) * cosh(xd) * cosh(yd) * cosh(yd));
		}
	}
}

}  // namespace

TEST(BlurEffectTest, BlurTwoDotsSmallRadius) {
	const float sigma = 3.0f;
	const int size = 32;
	const int x1 = 8;
	const int y1 = 8;
	const int x2 = 20;
	const int y2 = 10;

	float data[size * size], out_data[size * size], expected_data[size * size];
	memset(data, 0, sizeof(data));
	memset(expected_data, 0, sizeof(expected_data));

	data[y1 * size + x1] = 1.0f;
	data[y2 * size + x2] = 1.0f;

	add_blurred_point(expected_data, size, x1, y1, 1.0f, sigma);
	add_blurred_point(expected_data, size, x2, y2, 1.0f, sigma);

	EffectChainTester tester(data, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *blur_effect = tester.get_chain()->add_effect(new BlurEffect());
	ASSERT_TRUE(blur_effect->set_float("radius", sigma));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	// Set the limits a bit tighter than usual, since there is so little energy in here.
	expect_equal(expected_data, out_data, size, size, 1e-3, 1e-5);
}

TEST(BlurEffectTest, BlurTwoDotsLargeRadius) {
	const float sigma = 20.0f;  // Large enough that we will begin scaling.
	const int size = 256;
	const int x1 = 64;
	const int y1 = 64;
	const int x2 = 160;
	const int y2 = 120;

	static float data[size * size], out_data[size * size], expected_data[size * size];
	memset(data, 0, sizeof(data));
	memset(expected_data, 0, sizeof(expected_data));

	data[y1 * size + x1] = 128.0f;
	data[y2 * size + x2] = 128.0f;

	add_blurred_point(expected_data, size, x1, y1, 128.0f, sigma);
	add_blurred_point(expected_data, size, x2, y2, 128.0f, sigma);

	EffectChainTester tester(data, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *blur_effect = tester.get_chain()->add_effect(new BlurEffect());
	ASSERT_TRUE(blur_effect->set_float("radius", sigma));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, size, size, 0.1f, 1e-3);
}

TEST(BlurEffectTest, BlurTwoDotsSmallRadiusFewerTaps) {
	const float sigma = 3.0f;
	const int size = 32;
	const int x1 = 8;
	const int y1 = 8;
	const int x2 = 20;
	const int y2 = 10;

	float data[size * size], out_data[size * size], expected_data[size * size];
	memset(data, 0, sizeof(data));
	memset(expected_data, 0, sizeof(expected_data));

	data[y1 * size + x1] = 1.0f;
	data[y2 * size + x2] = 1.0f;

	add_blurred_point(expected_data, size, x1, y1, 1.0f, sigma);
	add_blurred_point(expected_data, size, x2, y2, 1.0f, sigma);

	EffectChainTester tester(data, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *blur_effect = tester.get_chain()->add_effect(new BlurEffect());
	ASSERT_TRUE(blur_effect->set_float("radius", sigma));
	ASSERT_TRUE(blur_effect->set_int("num_taps", 10));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	// Set the limits a bit tighter than usual, since there is so little energy in here.
	expect_equal(expected_data, out_data, size, size, 1e-3, 1e-5);
}

}  // namespace movit
