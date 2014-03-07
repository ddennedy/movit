// Unit tests for SaturationEffect.

#include <epoxy/gl.h>

#include "effect_chain.h"
#include "gtest/gtest.h"
#include "image_format.h"
#include "saturation_effect.h"
#include "test_util.h"

namespace movit {

TEST(SaturationEffectTest, SaturationOneIsPassThrough) {
	float data[] = {
		1.0f, 0.5f, 0.75f, 0.6f,
	};
	float out_data[4];
	EffectChainTester tester(data, 1, 1, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *saturation_effect = tester.get_chain()->add_effect(new SaturationEffect());
	ASSERT_TRUE(saturation_effect->set_float("saturation", 1.0f));
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(data, out_data, 1, 1);
}

TEST(SaturationEffectTest, SaturationZeroRemovesColorButPreservesAlpha) {
	float data[] = {
		0.0f, 0.0f, 0.0f, 1.0f,
		0.5f, 0.5f, 0.5f, 0.3f,
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 0.7f,
		0.0f, 0.0f, 1.0f, 1.0f,
	};
	float expected_data[] = {
		0.0f, 0.0f, 0.0f, 1.0f,
		0.5f, 0.5f, 0.5f, 0.3f,
		0.2126f, 0.2126f, 0.2126f, 1.0f,
		0.7152f, 0.7152f, 0.7152f, 0.7f,
		0.0722f, 0.0722f, 0.0722f, 1.0f,
	};

	float out_data[5 * 4];
	EffectChainTester tester(data, 5, 1, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *saturation_effect = tester.get_chain()->add_effect(new SaturationEffect());
	ASSERT_TRUE(saturation_effect->set_float("saturation", 0.0f));
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 4, 5);
}

TEST(SaturationEffectTest, DoubleSaturation) {
	float data[] = {
		0.0f, 0.0f, 0.0f, 1.0f,
		0.5f, 0.5f, 0.5f, 0.3f,
		0.3f, 0.1f, 0.1f, 1.0f,
	};
	float expected_data[] = {
		0.0f, 0.0f, 0.0f, 1.0f,
		0.5f, 0.5f, 0.5f, 0.3f,
		0.4570f, 0.0575f, 0.0575f, 1.0f,
	};

	float out_data[3 * 4];
	EffectChainTester tester(data, 3, 1, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *saturation_effect = tester.get_chain()->add_effect(new SaturationEffect());
	ASSERT_TRUE(saturation_effect->set_float("saturation", 2.0f));
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 4, 3);
}

}  // namespace movit
