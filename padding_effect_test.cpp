// Unit tests for PaddingEffect.

#include <epoxy/gl.h>
#include <stddef.h>

#include "effect_chain.h"
#include "flat_input.h"
#include "gtest/gtest.h"
#include "image_format.h"
#include "padding_effect.h"
#include "test_util.h"
#include "util.h"

namespace movit {

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

        EffectChainTester tester(nullptr, 4, 4);

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

	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);
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

        EffectChainTester tester(nullptr, 4, 4);

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

	RGBATuple border_color(1.0f, 1.0f, 1.0f, 1.0f);
	CHECK(effect->set_vec4("border_color", (float *)&border_color));

	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);
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

        EffectChainTester tester(nullptr, 1, 2);

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

	RGBATuple border_color(0.2f, 0.4f, 0.6f, 0.8f);  // Same as the pixel in data[].
	CHECK(effect->set_vec4("border_color", (float *)&border_color));

	tester.run(out_data, GL_RGBA, COLORSPACE_REC_601_625, GAMMA_REC_601, OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
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

        EffectChainTester tester(nullptr, 3, 3);

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

	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);
	expect_equal(expected_data, out_data, 3, 3);
}

TEST(PaddingEffectTest, NonIntegerOffset) {
	float data[4 * 1] = {
		0.25f, 0.50f, 0.75f, 1.0f,
	};
	float expected_data[5 * 2] = {
		0.1875f, 0.4375f, 0.6875f, 0.9375f, 0.25f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	};
	float out_data[5 * 2];

        EffectChainTester tester(nullptr, 5, 2);

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

	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);
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

        EffectChainTester tester(nullptr, 1, 1);

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

	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);
	expect_equal(expected_data, out_data, 1, 1);
}

TEST(PaddingEffectTest, AlphaIsCorrectEvenWithNonLinearInputsAndOutputs) {
	float data[2 * 1] = {
		1.0f,
		0.8f,
	};
	float expected_data[4 * 4] = {
		1.0f, 1.0f, 1.0f, 0.5f,
		1.0f, 1.0f, 1.0f, 1.0f,
		0.8f, 0.8f, 0.8f, 1.0f,
		1.0f, 1.0f, 1.0f, 0.5f,
	};
	float out_data[4 * 4];

	EffectChainTester tester(nullptr, 1, 4);

	ImageFormat format;
	format.color_space = COLORSPACE_REC_601_625;
	format.gamma_curve = GAMMA_REC_709;

	FlatInput *input = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, 1, 2);
	input->set_pixel_data(data);
	tester.get_chain()->add_input(input);

	Effect *effect = tester.get_chain()->add_effect(new PaddingEffect());
	CHECK(effect->set_int("width", 1));
	CHECK(effect->set_int("height", 4));
	CHECK(effect->set_float("left", 0.0f));
	CHECK(effect->set_float("top", 1.0f));

	RGBATuple border_color(1.0f, 1.0f, 1.0f, 0.5f);
	CHECK(effect->set_vec4("border_color", (float *)&border_color));

	tester.run(out_data, GL_RGBA, COLORSPACE_REC_601_625, GAMMA_REC_709, OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	expect_equal(expected_data, out_data, 4, 4);
}

TEST(PaddingEffectTest, RedBorder) {  // Not black nor white, but still a saturated primary.
	float data[2 * 1] = {
		1.0f,
		0.8f,
	};
	float expected_data[4 * 4] = {
		1.0f, 0.0f, 0.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
		0.8f, 0.8f, 0.8f, 1.0f,
		1.0f, 0.0f, 0.0f, 1.0f,
	};
	float out_data[4 * 4];

	EffectChainTester tester(nullptr, 1, 4);

	ImageFormat format;
	format.color_space = COLORSPACE_REC_601_625;
	format.gamma_curve = GAMMA_REC_709;

	FlatInput *input = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, 1, 2);
	input->set_pixel_data(data);
	tester.get_chain()->add_input(input);

	Effect *effect = tester.get_chain()->add_effect(new PaddingEffect());
	CHECK(effect->set_int("width", 1));
	CHECK(effect->set_int("height", 4));
	CHECK(effect->set_float("left", 0.0f));
	CHECK(effect->set_float("top", 1.0f));

	RGBATuple border_color(1.0f, 0.0f, 0.0f, 1.0f);
	CHECK(effect->set_vec4("border_color", (float *)&border_color));

	tester.run(out_data, GL_RGBA, COLORSPACE_REC_709, GAMMA_REC_709, OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	expect_equal(expected_data, out_data, 4, 4);
}

TEST(PaddingEffectTest, BorderOffsetTopAndBottom) {
	float data[2 * 2] = {
		1.0f, 0.5f,
		0.8f, 0.3f,
	};
	float expected_data[4 * 4] = {
		0.0f, 0.000f, 0.000f, 0.0f,
		0.0f, 0.750f, 0.375f, 0.0f,
		0.0f, 0.800f, 0.300f, 0.0f,
		0.0f, 0.200f, 0.075f, 0.0f,  // Repeated pixels, 25% opacity.
	};
	float out_data[4 * 4];

        EffectChainTester tester(nullptr, 4, 4);

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
	CHECK(effect->set_float("border_offset_top", 0.25f));
	CHECK(effect->set_float("border_offset_bottom", 0.25f));

	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);
	expect_equal(expected_data, out_data, 4, 4);
}

TEST(PaddingEffectTest, BorderOffsetLeftAndRight) {
	float data[3 * 2] = {
		1.0f, 0.5f, 0.6f,
		0.8f, 0.3f, 0.2f,
	};
	float expected_data[4 * 2] = {
		0.750f, 0.5f, 0.3f, 0.0f,
		0.600f, 0.3f, 0.1f, 0.0f
	};
	float out_data[4 * 2];

        EffectChainTester tester(nullptr, 4, 2);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	FlatInput *input = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, 3, 2);
	input->set_pixel_data(data);
	tester.get_chain()->add_input(input);

	Effect *effect = tester.get_chain()->add_effect(new PaddingEffect());
	CHECK(effect->set_int("width", 4));
	CHECK(effect->set_int("height", 2));
	CHECK(effect->set_float("left", 0.0f));
	CHECK(effect->set_float("top", 0.0f));
	CHECK(effect->set_float("border_offset_left", 0.25f));
	CHECK(effect->set_float("border_offset_right", -0.5f));

	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);
	expect_equal(expected_data, out_data, 4, 2);
}

}  // namespace movit
