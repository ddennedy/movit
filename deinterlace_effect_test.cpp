// Unit tests for DeinterlaceEffect.

#include <epoxy/gl.h>

#include <algorithm>

#include "effect_chain.h"
#include "gtest/gtest.h"
#include "image_format.h"
#include "input.h"
#include "deinterlace_effect.h"
#include "test_util.h"

using namespace std;

namespace movit {

TEST(DeinterlaceTest, ConstantColor) {
	float data[] = {
		0.3f, 0.3f,
		0.3f, 0.3f,
		0.3f, 0.3f,
	};
	float expected_data[] = {
		0.3f, 0.3f,
		0.3f, 0.3f,
		0.3f, 0.3f,
		0.3f, 0.3f,
		0.3f, 0.3f,
		0.3f, 0.3f,
	};
	float out_data[12];
	EffectChainTester tester(NULL, 2, 6);
	Effect *input1 = tester.add_input(data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, 2, 3);
	Effect *input2 = tester.add_input(data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, 2, 3);
	Effect *input3 = tester.add_input(data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, 2, 3);
	Effect *input4 = tester.add_input(data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, 2, 3);
	Effect *input5 = tester.add_input(data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, 2, 3);
	Effect *deinterlace_effect = tester.get_chain()->add_effect(new DeinterlaceEffect(), input1, input2, input3, input4, input5);

	ASSERT_TRUE(deinterlace_effect->set_int("current_field_position", 0));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);
	expect_equal(expected_data, out_data, 2, 6);

	ASSERT_TRUE(deinterlace_effect->set_int("current_field_position", 1));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);
	expect_equal(expected_data, out_data, 2, 6);
}

// Also tests that top/bottom change works like expected.
TEST(DeinterlaceTest, VerticalInterpolation) {
	const int width = 11;
	const int height = 2;
	float data[width * height] = {
		0.0f, 0.0f, 0.0f, 0.4f, 0.6f, 0.2f, 0.6f, 0.8f, 0.0f, 0.0f, 0.0f, 
		0.0f, 0.0f, 0.0f, 0.4f, 0.6f, 0.4f, 0.6f, 0.8f, 0.0f, 0.0f, 0.0f,   // Differs from previous.
	};
	float expected_data_top[width * height * 2] = {
		0.0f, 0.0f, 0.0f, 0.4f, 0.6f, 0.2f, 0.6f, 0.8f, 0.0f, 0.0f, 0.0f,   // Unchanged.
		0.0f, 0.0f, 0.0f, 0.4f, 0.6f, 0.3f, 0.6f, 0.8f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.4f, 0.6f, 0.4f, 0.6f, 0.8f, 0.0f, 0.0f, 0.0f,   // Unchanged.
		0.0f, 0.0f, 0.0f, 0.4f, 0.6f, 0.4f, 0.6f, 0.8f, 0.0f, 0.0f, 0.0f,   // Repeated.
	};
	float expected_data_bottom[width * height * 2] = {
		0.0f, 0.0f, 0.0f, 0.4f, 0.6f, 0.2f, 0.6f, 0.8f, 0.0f, 0.0f, 0.0f,   // Repeated
		0.0f, 0.0f, 0.0f, 0.4f, 0.6f, 0.2f, 0.6f, 0.8f, 0.0f, 0.0f, 0.0f,   // Unchanged.
		0.0f, 0.0f, 0.0f, 0.4f, 0.6f, 0.3f, 0.6f, 0.8f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.4f, 0.6f, 0.4f, 0.6f, 0.8f, 0.0f, 0.0f, 0.0f,   // Unchanged.
	};
	float neg_blowout_data[width * height];
	float pos_blowout_data[width * height];
	float out_data[width * height * 2];

	// Set previous and next fields to something so big that all the temporal checks
	// are effectively turned off.
	fill(neg_blowout_data, neg_blowout_data + width * height, -100.0f);
	fill(pos_blowout_data, pos_blowout_data + width * height,  100.0f);

	EffectChainTester tester(NULL, width, height * 2);
	Effect *input1 = tester.add_input(neg_blowout_data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, width, height);
	Effect *input2 = tester.add_input(neg_blowout_data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, width, height);
	Effect *input3 = tester.add_input(data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, width, height);
	Effect *input4 = tester.add_input(pos_blowout_data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, width, height);
	Effect *input5 = tester.add_input(pos_blowout_data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, width, height);
	Effect *deinterlace_effect = tester.get_chain()->add_effect(new DeinterlaceEffect(), input1, input2, input3, input4, input5);

	ASSERT_TRUE(deinterlace_effect->set_int("current_field_position", 0));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);
	expect_equal(expected_data_top, out_data, width, height * 2);

	ASSERT_TRUE(deinterlace_effect->set_int("current_field_position", 1));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);
	expect_equal(expected_data_bottom, out_data, width, height * 2);
}

TEST(DeinterlaceTest, DiagonalInterpolation) {
	const int width = 11;
	const int height = 3;
	float data[width * height] = {
		0.0f, 0.0f, 0.0f, 0.0f, 0.4f, 0.6f, 0.2f, 0.6f, 0.8f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.4f, 0.6f, 0.4f, 0.6f, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f,   // Offset two pixels, one value modified.
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.4f, 0.6f, 0.4f, 0.6f, 0.8f,   // Offset four the other way.
	};

	// Expected degrees are marked in comments. Mostly we want +45 for the second line
	// and -63 for the fourth, but due to the score being over three neighboring pixels,
	// sometimes it doesn't work ideally like that.
	float expected_data_top[width * height * 2] = {
		0.0f, 0.0f, 0.0f, 0.0f, 0.4f, 0.6f, 0.2f, 0.6f, 0.8f, 0.0f, 0.0f,   // Unchanged.
		// |    /     /     /     /     /     /     /     /     /    |
		// 0  +45   +45   +45   +45   +45   +45   +45   +45   +45    0
		0.0f, 0.0f, 0.0f, 0.4f, 0.6f, 0.3f, 0.6f, 0.8f, 0.0f, 0.0f, 0.0f,
		// | /     /     /     /     /     /     /     /     /       |  
		0.0f, 0.0f, 0.4f, 0.6f, 0.4f, 0.6f, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f,   // Unchanged.

		// 0  -45   -63   -63   -63   -63   -63   -63   +63!  +63!  +63!
		0.0f, 0.0f, 0.0f, 0.0f, 0.4f, 0.6f, 0.4f, 0.6f, 0.2f, 0.3f, 0.2f,

		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.4f, 0.6f, 0.4f, 0.6f, 0.8f,   // Unchanged.
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.4f, 0.6f, 0.4f, 0.6f, 0.8f,   // Repeated.
	};
	float neg_blowout_data[width * height];
	float pos_blowout_data[width * height];
	float out_data[width * height * 2];

	// Set previous and next fields to something so big that all the temporal checks
	// are effectively turned off.
	fill(neg_blowout_data, neg_blowout_data + width * height, -100.0f);
	fill(pos_blowout_data, pos_blowout_data + width * height,  100.0f);

	EffectChainTester tester(NULL, width, height * 2);
	Effect *input1 = tester.add_input(neg_blowout_data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, width, height);
	Effect *input2 = tester.add_input(neg_blowout_data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, width, height);
	Effect *input3 = tester.add_input(data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, width, height);
	Effect *input4 = tester.add_input(pos_blowout_data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, width, height);
	Effect *input5 = tester.add_input(pos_blowout_data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, width, height);
	Effect *deinterlace_effect = tester.get_chain()->add_effect(new DeinterlaceEffect(), input1, input2, input3, input4, input5);

	ASSERT_TRUE(deinterlace_effect->set_int("current_field_position", 0));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);
	expect_equal(expected_data_top, out_data, width, height * 2);
}

TEST(DeinterlaceTest, FlickerBox) {
	const int width = 4;
	const int height = 4;
	float white_data[width * height] = {
		1.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
	};
	float black_data[width * height] = {
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
	};
	float striped_data[width * height * 2] = {
		1.0f, 1.0f, 1.0f, 1.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
	};
	float out_data[width * height * 2];

	{
		EffectChainTester tester(NULL, width, height * 2);
		Effect *white_input = tester.add_input(white_data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, width, height);
		Effect *black_input = tester.add_input(black_data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, width, height);
		Effect *deinterlace_effect = tester.get_chain()->add_effect(new DeinterlaceEffect(), white_input, black_input, white_input, black_input, white_input);

		ASSERT_TRUE(deinterlace_effect->set_int("current_field_position", 0));
		tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);
		expect_equal(white_data, out_data, width, height);
		expect_equal(white_data, out_data + width * height, width, height);
	}

	{
		EffectChainTester tester(NULL, width, height * 2);
		Effect *white_input = tester.add_input(white_data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, width, height);
		Effect *black_input = tester.add_input(black_data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, width, height);
		Effect *deinterlace_effect = tester.get_chain()->add_effect(new DeinterlaceEffect(), white_input, black_input, white_input, black_input, white_input);

		ASSERT_TRUE(deinterlace_effect->set_int("enable_spatial_interlacing_check", 0));
		ASSERT_TRUE(deinterlace_effect->set_int("current_field_position", 0));
		tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);
		expect_equal(striped_data, out_data, width, height * 2);
	}
}

}  // namespace movit
