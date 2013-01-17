// Unit tests for AlphaMultiplicationEffect.

#include "test_util.h"
#include "flat_input.h"
#include "padding_effect.h"
#include "gtest/gtest.h"

TEST(PaddingEffectTest, SimpleCenter) {
	float data[2 * 2] = {
		1.0f, 0.5f,
		0.8f, 0.3f,
	};
	float expected_data[4 * 4] = {
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.5f, 0.0f,
		0.0f, 0.8f, 0.3f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
	};
	float out_data[4 * 4];

        EffectChainTester tester(NULL, 4, 4);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	FlatInput *input = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, 2, 2);
	input->set_pixel_data(data);
	tester.get_chain()->add_input(input);

	Effect *effect = tester.get_chain()->add_effect(new PaddingEffect());
	CHECK(effect->set_int("width", 4));
	CHECK(effect->set_int("height", 4));
	CHECK(effect->set_float("left", 1.0f));
	CHECK(effect->set_float("top", 1.0f));

	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_PREMULTIPLIED);
	expect_equal(expected_data, out_data, 4, 4);
}

TEST(PaddingEffectTest, WhiteBorderColor) {
	float data[2 * 2] = {
		1.0f, 0.5f,
		0.8f, 0.3f,
	};
	float expected_data[4 * 4] = {
		1.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 0.5f, 1.0f,
		1.0f, 0.8f, 0.3f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
	};
	float out_data[4 * 4];

        EffectChainTester tester(NULL, 4, 4);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	FlatInput *input = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, 2, 2);
	input->set_pixel_data(data);
	tester.get_chain()->add_input(input);

	Effect *effect = tester.get_chain()->add_effect(new PaddingEffect());
	CHECK(effect->set_int("width", 4));
	CHECK(effect->set_int("height", 4));
	CHECK(effect->set_float("left", 1.0f));
	CHECK(effect->set_float("top", 1.0f));

	RGBATriplet border_color(1.0f, 1.0f, 1.0f, 1.0f);
	CHECK(effect->set_vec4("border_color", (float *)&border_color));

	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_PREMULTIPLIED);
	expect_equal(expected_data, out_data, 4, 4);
}

TEST(PaddingEffectTest, BorderColorIsInLinearGamma) {
	float data[4 * 1] = {
		0.2f, 0.4f, 0.6f, 0.8f,
	};
	float expected_data[4 * 2] = {
		0.5005, 0.7051, 0.8677, 0.7998,  // Pixel from data[].
		0.5005, 0.7051, 0.8677, 0.7998,  // Pixel from the border color.
	};
	float out_data[4 * 2];

        EffectChainTester tester(NULL, 1, 2);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	FlatInput *input = new FlatInput(format, FORMAT_RGBA_PREMULTIPLIED_ALPHA, GL_FLOAT, 1, 1);
	input->set_pixel_data(data);
	tester.get_chain()->add_input(input);

	Effect *effect = tester.get_chain()->add_effect(new PaddingEffect());
	CHECK(effect->set_int("width", 1));
	CHECK(effect->set_int("height", 2));
	CHECK(effect->set_float("left", 0.0f));
	CHECK(effect->set_float("top", 0.0f));

	RGBATriplet border_color(0.2f, 0.4f, 0.6f, 0.8f);  // Same as the pixel in data[].
	CHECK(effect->set_vec4("border_color", (float *)&border_color));

	tester.run(out_data, GL_RGBA, COLORSPACE_REC_601_625, GAMMA_REC_601, OUTPUT_ALPHA_POSTMULTIPLIED);
	expect_equal(expected_data, out_data, 4, 2);
}

TEST(PaddingEffectTest, DifferentXAndYOffset) {
	float data[1 * 1] = {
		1.0f
	};
	float expected_data[3 * 3] = {
		0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 0.0f,
	};
	float out_data[3 * 3];

        EffectChainTester tester(NULL, 3, 3);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	FlatInput *input = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, 1, 1);
	input->set_pixel_data(data);
	tester.get_chain()->add_input(input);

	Effect *effect = tester.get_chain()->add_effect(new PaddingEffect());
	CHECK(effect->set_int("width", 3));
	CHECK(effect->set_int("height", 3));
	CHECK(effect->set_float("left", 2.0f));
	CHECK(effect->set_float("top", 1.0f));

	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_PREMULTIPLIED);
	expect_equal(expected_data, out_data, 3, 3);
}

TEST(PaddingEffectTest, NonIntegerOffset) {
	float data[4 * 1] = {
		0.25f, 0.50f, 0.75f, 1.0f,
	};
	// Note that the first pixel is completely blank, since the cutoff goes
	// at the immediate left of the texel.
	float expected_data[5 * 2] = {
		0.0f, 0.4375f, 0.6875f, 0.9375f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	};
	float out_data[5 * 2];

        EffectChainTester tester(NULL, 5, 2);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	FlatInput *input = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, 4, 1);
	input->set_pixel_data(data);
	tester.get_chain()->add_input(input);

	Effect *effect = tester.get_chain()->add_effect(new PaddingEffect());
	CHECK(effect->set_int("width", 5));
	CHECK(effect->set_int("height", 2));
	CHECK(effect->set_float("left", 0.25f));
	CHECK(effect->set_float("top", 0.0f));

	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_PREMULTIPLIED);
	expect_equal(expected_data, out_data, 5, 2);
}

TEST(PaddingEffectTest, Crop) {
	float data[2 * 2] = {
		1.0f, 0.5f,
		0.8f, 0.3f,
	};
	float expected_data[1 * 1] = {
		0.3f,
	};
	float out_data[1 * 1];

        EffectChainTester tester(NULL, 1, 1);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	FlatInput *input = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, 2, 2);
	input->set_pixel_data(data);
	tester.get_chain()->add_input(input);

	Effect *effect = tester.get_chain()->add_effect(new PaddingEffect());
	CHECK(effect->set_int("width", 1));
	CHECK(effect->set_int("height", 1));
	CHECK(effect->set_float("left", -1.0f));
	CHECK(effect->set_float("top", -1.0f));

	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_PREMULTIPLIED);
	expect_equal(expected_data, out_data, 1, 1);
}
