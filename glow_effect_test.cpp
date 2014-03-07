// Unit tests for GlowEffect.

#include <epoxy/gl.h>
#include <math.h>

#include "effect_chain.h"
#include "glow_effect.h"
#include "gtest/gtest.h"
#include "image_format.h"
#include "test_util.h"

namespace movit {

TEST(GlowEffectTest, NoAmountDoesNothing) {
	const int size = 4;

	float data[size * size] = {
		0.0, 1.0, 0.0, 1.0,
		0.0, 1.0, 1.0, 0.0,
		0.0, 0.5, 1.0, 0.5,
		0.0, 0.0, 0.0, 0.0,
	};
	float out_data[size * size];

	EffectChainTester tester(data, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *glow_effect = tester.get_chain()->add_effect(new GlowEffect());
	ASSERT_TRUE(glow_effect->set_float("radius", 2.0f));
	ASSERT_TRUE(glow_effect->set_float("blurred_mix_amount", 0.0f));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(data, out_data, size, size);
}

TEST(GlowEffectTest, SingleDot) {
	const int size = 13;
	const float sigma = 0.5f;
	const float amount = 0.2f;

	float data[] = {  // One single dot in the middle.
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 
	};
	float expected_data[size * size], out_data[size * size];

	// The output should be equal to the input, plus approximately a logistic blob.
	// From http://en.wikipedia.org/wiki/Logistic_distribution#Alternative_parameterization.
	const float c1 = M_PI / (sigma * 4 * sqrt(3.0f));
	const float c2 = M_PI / (sigma * 2.0 * sqrt(3.0f));

	for (int y = 0; y < size; ++y) {
		for (int x = 0; x < size; ++x) {
			float xd = c2 * (x - 6);
			float yd = c2 * (y - 6);
			expected_data[y * size + x] = data[y * size + x] +
				(amount * c1 * c1) / (cosh(xd) * cosh(xd) * cosh(yd) * cosh(yd));
		}
	}

	EffectChainTester tester(data, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *glow_effect = tester.get_chain()->add_effect(new GlowEffect());
	ASSERT_TRUE(glow_effect->set_float("radius", sigma));
	ASSERT_TRUE(glow_effect->set_float("blurred_mix_amount", amount));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, size, size, 0.1f, 1e-3);
}

TEST(GlowEffectTest, GlowsOntoZeroAlpha) {
	const int size = 7;
	const float sigma = 1.0f;
	const float amount = 1.0f;

	float data[4 * size] = {
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.5,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
	};
	float expected_data[4 * size] = {
		0.0, 1.0, 0.0, 0.002, 
		0.0, 1.0, 0.0, 0.014,
		0.0, 1.0, 0.0, 0.065, 
		0.0, 1.0, 0.0, 0.635, 
		0.0, 1.0, 0.0, 0.065, 
		0.0, 1.0, 0.0, 0.014,
		0.0, 1.0, 0.0, 0.002, 
	};

	float out_data[4 * size];

	EffectChainTester tester(data, 1, size, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *glow_effect = tester.get_chain()->add_effect(new GlowEffect());
	ASSERT_TRUE(glow_effect->set_float("radius", sigma));
	ASSERT_TRUE(glow_effect->set_float("blurred_mix_amount", amount));
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 4, size);
}

}  // namespace movit
