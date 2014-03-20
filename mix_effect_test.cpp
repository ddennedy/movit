// Unit tests for MixEffect.

#include <epoxy/gl.h>

#include "effect_chain.h"
#include "gtest/gtest.h"
#include "image_format.h"
#include "input.h"
#include "mix_effect.h"
#include "test_util.h"

namespace movit {

TEST(MixEffectTest, FiftyFiftyMix) {
	float data_a[] = {
		0.0f, 0.25f,
		0.75f, 1.0f,
	};
	float data_b[] = {
		1.0f, 0.5f,
		0.75f, 0.6f,
	};
	float expected_data[] = {
		0.5f, 0.375f,
		0.75f, 0.8f,
	};
	float out_data[4];
	EffectChainTester tester(data_a, 2, 2, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);

	Effect *mix_effect = tester.get_chain()->add_effect(new MixEffect(), input1, input2);
	ASSERT_TRUE(mix_effect->set_float("strength_first", 0.5f));
	ASSERT_TRUE(mix_effect->set_float("strength_second", 0.5f));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 2, 2);
}

TEST(MixEffectTest, OnlyA) {
	float data_a[] = {
		0.0f, 0.25f,
		0.75f, 1.0f,
	};
	float data_b[] = {
		1.0f, 0.5f,
		0.75f, 0.6f,
	};
	float out_data[4];
	EffectChainTester tester(data_a, 2, 2, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);

	Effect *mix_effect = tester.get_chain()->add_effect(new MixEffect(), input1, input2);
	ASSERT_TRUE(mix_effect->set_float("strength_first", 1.0f));
	ASSERT_TRUE(mix_effect->set_float("strength_second", 0.0f));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(data_a, out_data, 2, 2);
}

TEST(MixEffectTest, DoesNotSumToOne) {
	float data_a[] = {
		1.0f, 0.5f, 0.75f, 0.333f,
	};
	float data_b[] = {
		1.0f, 0.25f, 0.15f, 0.333f,
	};

	// The fact that the RGB values don't sum but get averaged here might
	// actually be a surprising result, but when you think of it,
	// it does make physical sense.
	float expected_data[] = {
		1.0f, 0.375f, 0.45f, 0.666f,
	};

	float out_data[4];
	EffectChainTester tester(data_a, 1, 1, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);

	Effect *mix_effect = tester.get_chain()->add_effect(new MixEffect(), input1, input2);
	ASSERT_TRUE(mix_effect->set_float("strength_first", 1.0f));
	ASSERT_TRUE(mix_effect->set_float("strength_second", 1.0f));
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 4, 1);
}

TEST(MixEffectTest, AdditiveBlendingWorksForBothTotallyOpaqueAndPartiallyTranslucent) {
	float data_a[] = {
		0.0f, 0.5f, 0.75f, 1.0f,
		1.0f, 1.0f, 1.0f, 0.2f,
	};
	float data_b[] = {
		1.0f, 0.25f, 0.15f, 1.0f,
		1.0f, 1.0f, 1.0f, 0.5f,
	};

	float expected_data[] = {
		1.0f, 0.75f, 0.9f, 1.0f,
		1.0f, 1.0f, 1.0f, 0.7f,
	};

	float out_data[8];
	EffectChainTester tester(data_a, 1, 2, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);

	Effect *mix_effect = tester.get_chain()->add_effect(new MixEffect(), input1, input2);
	ASSERT_TRUE(mix_effect->set_float("strength_first", 1.0f));
	ASSERT_TRUE(mix_effect->set_float("strength_second", 1.0f));
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 4, 2);
}

TEST(MixEffectTest, MixesLinearlyDespitesRGBInputsAndOutputs) {
	float data_a[] = {
		0.0f, 0.25f,
		0.75f, 1.0f,
	};
	float data_b[] = {
		0.0f, 0.0f,
		0.0f, 0.0f,
	};
	float expected_data[] = {  // sRGB(0.5 * inv_sRGB(a)).
		0.00000f, 0.17349f,
		0.54807f, 0.73536f,
	};
	float out_data[4];
	EffectChainTester tester(data_a, 2, 2, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_sRGB);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_sRGB);

	Effect *mix_effect = tester.get_chain()->add_effect(new MixEffect(), input1, input2);
	ASSERT_TRUE(mix_effect->set_float("strength_first", 0.5f));
	ASSERT_TRUE(mix_effect->set_float("strength_second", 0.5f));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_sRGB);

	expect_equal(expected_data, out_data, 2, 2);
}

}  // namespace movit
