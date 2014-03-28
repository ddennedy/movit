// Unit tests for LumaMixEffect.

#include <epoxy/gl.h>

#include "effect_chain.h"
#include "gtest/gtest.h"
#include "image_format.h"
#include "input.h"
#include "luma_mix_effect.h"
#include "test_util.h"

namespace movit {

TEST(LumaMixEffectTest, HardWipe) {
	float data_a[] = {
		0.0f, 0.25f,
		0.75f, 1.0f,
	};
	float data_b[] = {
		1.0f, 0.5f,
		0.65f, 0.6f,
	};
	float data_luma[] = {
		0.0f, 0.25f,
		0.5f, 0.75f,
	};

	EffectChainTester tester(data_a, 2, 2, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input3 = tester.add_input(data_luma, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);

	Effect *luma_mix_effect = tester.get_chain()->add_effect(new LumaMixEffect(), input1, input2, input3);
	ASSERT_TRUE(luma_mix_effect->set_float("transition_width", 100000.0f));

	float out_data[4];
	ASSERT_TRUE(luma_mix_effect->set_float("progress", 0.0f));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);
	expect_equal(data_a, out_data, 2, 2);

	// Lower right from B, the rest from A.
	float expected_data_049[] = {
		0.0f, 0.25f,
		0.75f, 0.6f,
	};
	ASSERT_TRUE(luma_mix_effect->set_float("progress", 0.49f));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);
	expect_equal(expected_data_049, out_data, 2, 2);

	// Lower two from B, the rest from A.
	float expected_data_051[] = {
		0.0f, 0.25f,
		0.65f, 0.6f,
	};
	ASSERT_TRUE(luma_mix_effect->set_float("progress", 0.51f));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);
	expect_equal(expected_data_051, out_data, 2, 2);

	ASSERT_TRUE(luma_mix_effect->set_float("progress", 1.0f));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);
	expect_equal(data_b, out_data, 2, 2);
}

TEST(LumaMixEffectTest, SoftWipeHalfWayThrough) {
	float data_a[] = {
		0.0f, 0.25f,
		0.75f, 1.0f,
	};
	float data_b[] = {
		1.0f, 0.5f,
		0.65f, 0.6f,
	};
	float data_luma[] = {
		0.0f, 0.25f,
		0.5f, 0.75f,
	};
	// At this point, the luma range and the mix range should exactly line up,
	// so we get a straight-up fade by luma.
	float expected_data[] = {
		data_a[0] + (data_b[0] - data_a[0]) * data_luma[0],
		data_a[1] + (data_b[1] - data_a[1]) * data_luma[1],
		data_a[2] + (data_b[2] - data_a[2]) * data_luma[2],
		data_a[3] + (data_b[3] - data_a[3]) * data_luma[3],
	};
	float out_data[4];

	EffectChainTester tester(data_a, 2, 2, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input3 = tester.add_input(data_luma, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);

	Effect *luma_mix_effect = tester.get_chain()->add_effect(new LumaMixEffect(), input1, input2, input3);
	ASSERT_TRUE(luma_mix_effect->set_float("transition_width", 1.0f));
	ASSERT_TRUE(luma_mix_effect->set_float("progress", 0.5f));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);
	expect_equal(expected_data, out_data, 2, 2);
}

TEST(LumaMixEffectTest, Inverse) {
	float data_a[] = {
		0.0f, 0.25f,
		0.75f, 1.0f,
	};
	float data_b[] = {
		1.0f, 0.5f,
		0.65f, 0.6f,
	};
	float data_luma[] = {
		0.0f, 0.25f,
		0.5f, 0.75f,
	};

	EffectChainTester tester(data_a, 2, 2, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input3 = tester.add_input(data_luma, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);

	Effect *luma_mix_effect = tester.get_chain()->add_effect(new LumaMixEffect(), input1, input2, input3);
	ASSERT_TRUE(luma_mix_effect->set_float("transition_width", 100000.0f));
	ASSERT_TRUE(luma_mix_effect->set_int("inverse", 1));

	// Inverse is not the same as reverse, so progress=0 should behave identically
	// as HardWipe, ie. everything should be from A.
	float out_data[4];
	ASSERT_TRUE(luma_mix_effect->set_float("progress", 0.0f));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);
	expect_equal(data_a, out_data, 2, 2);

	// Lower two from A, the rest from B.
	float expected_data_049[] = {
		1.0f, 0.5f,
		0.75f, 1.0f,
	};
	ASSERT_TRUE(luma_mix_effect->set_float("progress", 0.49f));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);
	expect_equal(expected_data_049, out_data, 2, 2);
}

}  // namespace movit
