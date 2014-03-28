// Unit tests for DiffusionEffect.

#include <epoxy/gl.h>

#include "diffusion_effect.h"
#include "effect_chain.h"
#include "gtest/gtest.h"
#include "image_format.h"
#include "test_util.h"

namespace movit {

TEST(DiffusionEffectTest, IdentityTransformDoesNothing) {
	const int size = 4;

	float data[size * size] = {
		0.0, 1.0, 0.0, 1.0,
		0.0, 1.0, 1.0, 0.0,
		0.0, 0.5, 1.0, 0.5,
		0.0, 0.0, 0.0, 0.0,
	};
	float out_data[size * size];

	EffectChainTester tester(data, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *diffusion_effect = tester.get_chain()->add_effect(new DiffusionEffect());
	ASSERT_TRUE(diffusion_effect->set_float("radius", 0.0f));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(data, out_data, size, size);
}

TEST(DiffusionEffectTest, FlattensOutWhitePyramid) {
	const int size = 9;

	float data[size * size] = {
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.5, 0.5, 0.5, 0.5, 0.5, 0.0, 0.0,
		0.0, 0.0, 0.5, 0.7, 0.7, 0.7, 0.5, 0.0, 0.0,
		0.0, 0.0, 0.5, 0.7, 1.0, 0.7, 0.5, 0.0, 0.0,
		0.0, 0.0, 0.5, 0.7, 0.7, 0.7, 0.5, 0.0, 0.0,
		0.0, 0.0, 0.5, 0.5, 0.5, 0.5, 0.5, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
	};
	float expected_data[size * size] = {
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.4, 0.4, 0.4, 0.4, 0.4, 0.0, 0.0,
		0.0, 0.0, 0.4, 0.5, 0.5, 0.5, 0.4, 0.0, 0.0,
		0.0, 0.0, 0.4, 0.5, 0.6, 0.5, 0.4, 0.0, 0.0,
		0.0, 0.0, 0.4, 0.5, 0.5, 0.5, 0.4, 0.0, 0.0,
		0.0, 0.0, 0.4, 0.4, 0.4, 0.4, 0.4, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
	};
	float out_data[size * size];

	EffectChainTester tester(data, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *diffusion_effect = tester.get_chain()->add_effect(new DiffusionEffect());
	ASSERT_TRUE(diffusion_effect->set_float("radius", 2.0f));
	ASSERT_TRUE(diffusion_effect->set_float("blurred_mix_amount", 0.7f));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, size, size, 0.05f, 0.002);
}

}  // namespace movit
