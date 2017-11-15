// Unit tests for SliceEffect.

#include <epoxy/gl.h>

#include "effect_chain.h"
#include "flat_input.h"
#include "gtest/gtest.h"
#include "image_format.h"
#include "input.h"
#include "slice_effect.h"
#include "test_util.h"

namespace movit {

TEST(SliceEffectTest, Identity) {
	const int size = 3, output_size = 4;
	float data[size * size] = {
		0.0f, 0.1f, 0.2f,
		0.4f, 0.3f, 0.8f,
		0.5f, 0.2f, 0.1f,
	};
	float expected_data[output_size * size] = {
		0.0f, 0.1f, 0.2f, 0.2f,
		0.4f, 0.3f, 0.8f, 0.8f,
		0.5f, 0.2f, 0.1f, 0.1f,
	};
	float out_data[output_size * size];

	EffectChainTester tester(nullptr, output_size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.add_input(data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, size, size);

	Effect *slice_effect = tester.get_chain()->add_effect(new SliceEffect());
	ASSERT_TRUE(slice_effect->set_int("input_slice_size", 2));
	ASSERT_TRUE(slice_effect->set_int("output_slice_size", 2));
	ASSERT_TRUE(slice_effect->set_int("direction", SliceEffect::HORIZONTAL));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, output_size, size);
}

TEST(SliceEffectTest, HorizontalOverlap) {
	float data[5 * 2] = {
		0.0f, 0.1f,  0.2f, 0.3f,  0.4f,
		0.4f, 0.3f,  0.2f, 0.1f,  0.0f,
	};
	float expected_data[9 * 2] = {
		0.0f, 0.1f, 0.2f,  0.2f, 0.3f, 0.4f,  0.4f, 0.4f, 0.4f,
		0.4f, 0.3f, 0.2f,  0.2f, 0.1f, 0.0f,  0.0f, 0.0f, 0.0f,
	};
	float out_data[9 * 2];

	EffectChainTester tester(nullptr, 9, 2, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.add_input(data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, 5, 2);

	Effect *slice_effect = tester.get_chain()->add_effect(new SliceEffect());
	ASSERT_TRUE(slice_effect->set_int("input_slice_size", 2));
	ASSERT_TRUE(slice_effect->set_int("output_slice_size", 3));
	ASSERT_TRUE(slice_effect->set_int("direction", SliceEffect::HORIZONTAL));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 9, 2);
}

TEST(SliceEffectTest, HorizontalDiscard) {
	float data[6 * 2] = {
		0.0f, 0.1f, 0.2f,  0.2f, 0.3f, 0.4f,
		0.4f, 0.3f, 0.2f,  0.2f, 0.1f, 0.0f,
	};
	float expected_data[4 * 2] = {
		0.0f, 0.1f,  0.2f, 0.3f,
		0.4f, 0.3f,  0.2f, 0.1f,
	};
	float out_data[4 * 2];

	EffectChainTester tester(nullptr, 4, 2, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.add_input(data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, 6, 2);

	Effect *slice_effect = tester.get_chain()->add_effect(new SliceEffect());
	ASSERT_TRUE(slice_effect->set_int("input_slice_size", 3));
	ASSERT_TRUE(slice_effect->set_int("output_slice_size", 2));
	ASSERT_TRUE(slice_effect->set_int("direction", SliceEffect::HORIZONTAL));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 4, 2);
}

TEST(SliceEffectTest, HorizontalOverlapWithOffset) {
	float data[5 * 2] = {
		/* 0.0f, */ 0.0f,  0.1f, 0.2f,  0.3f, 0.4f,
		/* 0.4f, */ 0.4f,  0.3f, 0.2f,  0.1f, 0.0f,
	};
	float expected_data[9 * 2] = {
		0.0f, 0.0f, 0.1f,  0.1f, 0.2f, 0.3f,  0.3f, 0.4f, 0.4f,
		0.4f, 0.4f, 0.3f,  0.3f, 0.2f, 0.1f,  0.1f, 0.0f, 0.0f,
	};
	float out_data[9 * 2];

	EffectChainTester tester(nullptr, 9, 2, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.add_input(data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, 5, 2);

	Effect *slice_effect = tester.get_chain()->add_effect(new SliceEffect());
	ASSERT_TRUE(slice_effect->set_int("input_slice_size", 2));
	ASSERT_TRUE(slice_effect->set_int("output_slice_size", 3));
	ASSERT_TRUE(slice_effect->set_int("offset", -1));
	ASSERT_TRUE(slice_effect->set_int("direction", SliceEffect::HORIZONTAL));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 9, 2);
}

TEST(SliceEffectTest, VerticalOverlapSlicesFromTop) {
	float data[2 * 3] = {
		0.0f, 0.1f,
		0.4f, 0.3f,

		0.6f, 0.2f,
	};
	float expected_data[2 * 6] = {
		0.0f, 0.1f,
		0.4f, 0.3f,
		0.6f, 0.2f,

		0.6f, 0.2f,
		0.6f, 0.2f,
		0.6f, 0.2f,
	};
	float out_data[2 * 6];

	EffectChainTester tester(nullptr, 2, 6, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.add_input(data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, 2, 3);

	Effect *slice_effect = tester.get_chain()->add_effect(new SliceEffect());
	ASSERT_TRUE(slice_effect->set_int("input_slice_size", 2));
	ASSERT_TRUE(slice_effect->set_int("output_slice_size", 3));
	ASSERT_TRUE(slice_effect->set_int("direction", SliceEffect::VERTICAL));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 2, 6);
}

TEST(SliceEffectTest, VerticalOverlapOffsetsFromTop) {
	float data[2 * 3] = {
		0.0f, 0.1f,
		0.4f, 0.3f,

		0.6f, 0.2f,
	};
	float expected_data[2 * 6] = {
		0.4f, 0.3f,
		0.6f, 0.2f,
		0.6f, 0.2f,

		0.6f, 0.2f,
		0.6f, 0.2f,
		0.6f, 0.2f,
	};
	float out_data[2 * 6];

	EffectChainTester tester(nullptr, 2, 6, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.add_input(data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, 2, 3);

	Effect *slice_effect = tester.get_chain()->add_effect(new SliceEffect());
	ASSERT_TRUE(slice_effect->set_int("input_slice_size", 2));
	ASSERT_TRUE(slice_effect->set_int("output_slice_size", 3));
	ASSERT_TRUE(slice_effect->set_int("offset", 1));
	ASSERT_TRUE(slice_effect->set_int("direction", SliceEffect::VERTICAL));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 2, 6);
}

}  // namespace movit
