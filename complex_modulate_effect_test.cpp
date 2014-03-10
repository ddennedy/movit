// Unit tests for ComplexModulateEffect.

#include <epoxy/gl.h>

#include "effect_chain.h"
#include "gtest/gtest.h"
#include "complex_modulate_effect.h"
#include "image_format.h"
#include "input.h"
#include "test_util.h"

namespace movit {

TEST(ComplexModulateEffectTest, Identity) {
	const int size = 3;
	float data_a[size * 4] = {
		0.0f, 0.1f, 0.2f, 0.1f,
		0.4f, 0.3f, 0.8f, 2.0f,
		0.5f, 0.2f, 0.1f, 0.0f,
	};
	float data_b[size * 2] = {
		1.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 0.0f,
	};
	float out_data[size * 4];

	EffectChainTester tester(data_a, 1, size, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_RG, COLORSPACE_sRGB, GAMMA_LINEAR);

	tester.get_chain()->add_effect(new ComplexModulateEffect(), input1, input2);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	expect_equal(data_a, out_data, 4, size);
}

TEST(ComplexModulateEffectTest, ComplexMultiplication) {
	const int size = 2;
	float data_a[size * 4] = {
		0.0f, 0.1f, 0.2f, 0.1f,
		0.4f, 0.3f, 0.8f, 2.0f,
	};
	float data_b[size * 2] = {
		0.0f,  1.0f,
		0.5f, -0.8f,
	};
	float expected_data[size * 4] = {
		-0.1f,   0.0f,  -0.1f, 0.2f,
		 0.44f, -0.17f,  2.0f, 0.36f,
	};
	float out_data[size * 4];

	EffectChainTester tester(data_a, 1, size, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_RG, COLORSPACE_sRGB, GAMMA_LINEAR);

	tester.get_chain()->add_effect(new ComplexModulateEffect(), input1, input2);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	expect_equal(expected_data, out_data, 4, size);
}

TEST(ComplexModulateEffectTest, Repeat) {
	const int size = 2, repeats = 3;
	float data_a[size * repeats * 4] = {
		0.0f, 0.1f, 0.2f, 0.3f,
		1.0f, 1.1f, 1.2f, 1.3f,
		2.0f, 2.1f, 2.2f, 2.3f,
		3.0f, 3.1f, 3.2f, 3.3f,
		4.0f, 4.1f, 4.2f, 4.3f,
		5.0f, 5.1f, 5.2f, 5.3f,
	};
	float data_b[size * 2] = {
		1.0f,  0.0f,
		0.0f, -1.0f,
	};
	float expected_data[size * repeats * 4] = {
		0.0f,  0.1f, 0.2f,  0.3f,
		1.1f, -1.0f, 1.3f, -1.2f,
		2.0f,  2.1f, 2.2f,  2.3f,
		3.1f, -3.0f, 3.3f, -3.2f,
		4.0f,  4.1f, 4.2f,  4.3f,
		5.1f, -5.0f, 5.3f, -5.2f,
	};
	float out_data[size * repeats * 4];

	EffectChainTester tester(data_a, 1, repeats * size, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_RG, COLORSPACE_sRGB, GAMMA_LINEAR, 1, size);

	Effect *effect = tester.get_chain()->add_effect(new ComplexModulateEffect(), input1, input2);
	ASSERT_TRUE(effect->set_int("num_repeats_y", repeats));
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	expect_equal(expected_data, out_data, 4, size * repeats);
}

}  // namespace movit
