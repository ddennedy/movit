// Unit tests for WhiteBalanceEffect.

#include <epoxy/gl.h>

#include "effect_chain.h"
#include "gtest/gtest.h"
#include "image_format.h"
#include "test_util.h"
#include "white_balance_effect.h"

namespace movit {

TEST(WhiteBalanceEffectTest, GrayNeutralDoesNothing) {
	float data[] = {
		0.0f, 0.0f, 0.0f, 1.0f,
		0.5f, 0.5f, 0.5f, 0.3f,
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 0.7f,
		0.0f, 0.0f, 1.0f, 1.0f,
	};
	float neutral[] = {
		0.5f, 0.5f, 0.5f
	};

	float out_data[5 * 4];
	EffectChainTester tester(data, 1, 5, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *white_balance_effect = tester.get_chain()->add_effect(new WhiteBalanceEffect());
	ASSERT_TRUE(white_balance_effect->set_vec3("neutral_color", neutral));
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(data, out_data, 4, 5);
}

TEST(WhiteBalanceEffectTest, SettingReddishNeutralColorNeutralizesReddishColor) {
	float data[] = {
		0.0f, 0.0f, 0.0f, 1.0f,
		0.6f, 0.5f, 0.5f, 1.0f,
		0.5f, 0.5f, 0.5f, 1.0f,
	};
	float neutral[] = {
		0.6f, 0.5f, 0.5f
	};

	float out_data[3 * 4];
	EffectChainTester tester(data, 1, 3, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *white_balance_effect = tester.get_chain()->add_effect(new WhiteBalanceEffect());
	ASSERT_TRUE(white_balance_effect->set_vec3("neutral_color", neutral));
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	// Black should stay black.
	EXPECT_FLOAT_EQ(0.0f, out_data[4 * 0 + 0]);
	EXPECT_FLOAT_EQ(0.0f, out_data[4 * 0 + 1]);
	EXPECT_FLOAT_EQ(0.0f, out_data[4 * 0 + 2]);
	EXPECT_FLOAT_EQ(1.0f, out_data[4 * 0 + 3]);

	// The neutral color should now have R=G=B.
	EXPECT_NEAR(out_data[4 * 1 + 0], out_data[4 * 1 + 1], 0.001);
	EXPECT_NEAR(out_data[4 * 1 + 0], out_data[4 * 1 + 2], 0.001);
	EXPECT_FLOAT_EQ(1.0f, out_data[4 * 1 + 3]);

	// It should also have kept its luminance.
	float old_luminance = 0.2126 * data[4 * 1 + 0] +
		0.7152 * data[4 * 1 + 1] +
		0.0722 * data[4 * 1 + 2];	
	float new_luminance = 0.2126 * out_data[4 * 1 + 0] +
		0.7152 * out_data[4 * 1 + 1] +
		0.0722 * out_data[4 * 1 + 2];
	EXPECT_NEAR(old_luminance, new_luminance, 0.001);

	// Finally, the old gray should now have significantly less red than green and blue.
	EXPECT_GT(out_data[4 * 2 + 1] - out_data[4 * 2 + 0], 0.05);
	EXPECT_GT(out_data[4 * 2 + 2] - out_data[4 * 2 + 0], 0.05);
}

TEST(WhiteBalanceEffectTest, HigherColorTemperatureIncreasesBlue) {
	float data[] = {
		0.0f, 0.0f, 0.0f, 1.0f,
		0.5f, 0.5f, 0.5f, 1.0f,
	};

	float out_data[2 * 4];
	EffectChainTester tester(data, 1, 2, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *white_balance_effect = tester.get_chain()->add_effect(new WhiteBalanceEffect());
	ASSERT_TRUE(white_balance_effect->set_float("output_color_temperature", 10000.0f));
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	// Black should stay black.
	EXPECT_FLOAT_EQ(0.0f, out_data[4 * 0 + 0]);
	EXPECT_FLOAT_EQ(0.0f, out_data[4 * 0 + 1]);
	EXPECT_FLOAT_EQ(0.0f, out_data[4 * 0 + 2]);
	EXPECT_FLOAT_EQ(1.0f, out_data[4 * 0 + 3]);

	// The neutral color should have kept its luminance.
	float old_luminance = 0.2126 * data[4 * 1 + 0] +
		0.7152 * data[4 * 1 + 1] +
		0.0722 * data[4 * 1 + 2];	
	float new_luminance = 0.2126 * out_data[4 * 1 + 0] +
		0.7152 * out_data[4 * 1 + 1] +
		0.0722 * out_data[4 * 1 + 2];
	EXPECT_NEAR(old_luminance, new_luminance, 0.001);

	// It should also have significantly more blue then green,
	// and significantly less red than green.
	EXPECT_GT(out_data[4 * 1 + 2] - out_data[4 * 1 + 1], 0.05);
	EXPECT_GT(out_data[4 * 1 + 1] - out_data[4 * 1 + 0], 0.05);
}

}  // namespace movit
