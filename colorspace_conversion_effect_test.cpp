// Unit tests for ColorspaceConversionEffect.

#include <epoxy/gl.h>

#include "colorspace_conversion_effect.h"
#include "gtest/gtest.h"
#include "test_util.h"

namespace movit {

TEST(ColorspaceConversionEffectTest, Reversible) {
	float data[] = {
		0.0f, 0.0f, 0.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
		0.0f, 1.0f, 1.0f, 0.5f,
	};
	float temp_data[4 * 6], out_data[4 * 6];

	{
		EffectChainTester tester(data, 1, 6, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
		tester.run(temp_data, GL_RGBA, COLORSPACE_REC_601_525, GAMMA_LINEAR);
	}
	{
		EffectChainTester tester(temp_data, 1, 6, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_REC_601_525, GAMMA_LINEAR);
		tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);
	}

	expect_equal(data, out_data, 4, 6);
}

TEST(ColorspaceConversionEffectTest, sRGB_Primaries) {
	float data[] = {
		0.0f, 0.0f, 0.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
	};
	float out_data[4 * 5];

	EffectChainTester tester(data, 1, 5, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.run(out_data, GL_RGBA, COLORSPACE_XYZ, GAMMA_LINEAR);

	// Black should stay black.
	EXPECT_FLOAT_EQ(0.0f, out_data[0 * 4 + 0]);
	EXPECT_FLOAT_EQ(0.0f, out_data[0 * 4 + 1]);
	EXPECT_FLOAT_EQ(0.0f, out_data[0 * 4 + 2]);
	EXPECT_FLOAT_EQ(1.0f, out_data[0 * 4 + 3]);

	// White point should be D65.
	// XYZ values from http://en.wikipedia.org/wiki/CIE_Standard_Illuminant_D65.
	EXPECT_NEAR(0.9505, out_data[1 * 4 + 0], 1e-3);
	EXPECT_NEAR(1.0000, out_data[1 * 4 + 1], 1e-3);
	EXPECT_NEAR(1.0889, out_data[1 * 4 + 2], 1e-3);
	EXPECT_FLOAT_EQ(1.0f, out_data[1 * 4 + 3]);

	float white_xyz_sum = out_data[1 * 4 + 0] + out_data[1 * 4 + 1] + out_data[1 * 4 + 2];
	float white_x = out_data[1 * 4 + 0] / white_xyz_sum;
	float white_y = out_data[1 * 4 + 1] / white_xyz_sum;
	EXPECT_NEAR(0.3127, white_x, 1e-3);
	EXPECT_NEAR(0.3290, white_y, 1e-3);
	EXPECT_FLOAT_EQ(1.0f, out_data[1 * 4 + 3]);

	// Convert the primaries from XYZ to xyz, and compare to the references
	// given by Rec. 709 (which are shared with sRGB).

	float red_xyz_sum = out_data[2 * 4 + 0] + out_data[2 * 4 + 1] + out_data[2 * 4 + 2];
	float red_x = out_data[2 * 4 + 0] / red_xyz_sum;
	float red_y = out_data[2 * 4 + 1] / red_xyz_sum;
	EXPECT_NEAR(0.640, red_x, 1e-3);
	EXPECT_NEAR(0.330, red_y, 1e-3);
	EXPECT_FLOAT_EQ(1.0f, out_data[2 * 4 + 3]);

	float green_xyz_sum = out_data[3 * 4 + 0] + out_data[3 * 4 + 1] + out_data[3 * 4 + 2];
	float green_x = out_data[3 * 4 + 0] / green_xyz_sum;
	float green_y = out_data[3 * 4 + 1] / green_xyz_sum;
	EXPECT_NEAR(0.300, green_x, 1e-3);
	EXPECT_NEAR(0.600, green_y, 1e-3);
	EXPECT_FLOAT_EQ(1.0f, out_data[3 * 4 + 3]);

	float blue_xyz_sum = out_data[4 * 4 + 0] + out_data[4 * 4 + 1] + out_data[4 * 4 + 2];
	float blue_x = out_data[4 * 4 + 0] / blue_xyz_sum;
	float blue_y = out_data[4 * 4 + 1] / blue_xyz_sum;
	EXPECT_NEAR(0.150, blue_x, 1e-3);
	EXPECT_NEAR(0.060, blue_y, 1e-3);
	EXPECT_FLOAT_EQ(1.0f, out_data[4 * 4 + 3]);
}

TEST(ColorspaceConversionEffectTest, Rec601_525_Primaries) {
	float data[] = {
		0.0f, 0.0f, 0.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
	};
	float out_data[4 * 5];

	EffectChainTester tester(data, 1, 5, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_REC_601_525, GAMMA_LINEAR);
	tester.run(out_data, GL_RGBA, COLORSPACE_XYZ, GAMMA_LINEAR);

	// Black should stay black.
	EXPECT_FLOAT_EQ(0.0f, out_data[0 * 4 + 0]);
	EXPECT_FLOAT_EQ(0.0f, out_data[0 * 4 + 1]);
	EXPECT_FLOAT_EQ(0.0f, out_data[0 * 4 + 2]);
	EXPECT_FLOAT_EQ(1.0f, out_data[0 * 4 + 3]);

	// Convert the primaries from XYZ to xyz, and compare to the references
	// given by Rec. 601.
	float white_xyz_sum = out_data[1 * 4 + 0] + out_data[1 * 4 + 1] + out_data[1 * 4 + 2];
	float white_x = out_data[1 * 4 + 0] / white_xyz_sum;
	float white_y = out_data[1 * 4 + 1] / white_xyz_sum;
	EXPECT_NEAR(0.3127, white_x, 1e-3);
	EXPECT_NEAR(0.3290, white_y, 1e-3);
	EXPECT_FLOAT_EQ(1.0f, out_data[1 * 4 + 3]);

	float red_xyz_sum = out_data[2 * 4 + 0] + out_data[2 * 4 + 1] + out_data[2 * 4 + 2];
	float red_x = out_data[2 * 4 + 0] / red_xyz_sum;
	float red_y = out_data[2 * 4 + 1] / red_xyz_sum;
	EXPECT_NEAR(0.630, red_x, 1e-3);
	EXPECT_NEAR(0.340, red_y, 1e-3);
	EXPECT_FLOAT_EQ(1.0f, out_data[2 * 4 + 3]);

	float green_xyz_sum = out_data[3 * 4 + 0] + out_data[3 * 4 + 1] + out_data[3 * 4 + 2];
	float green_x = out_data[3 * 4 + 0] / green_xyz_sum;
	float green_y = out_data[3 * 4 + 1] / green_xyz_sum;
	EXPECT_NEAR(0.310, green_x, 1e-3);
	EXPECT_NEAR(0.595, green_y, 1e-3);
	EXPECT_FLOAT_EQ(1.0f, out_data[3 * 4 + 3]);

	float blue_xyz_sum = out_data[4 * 4 + 0] + out_data[4 * 4 + 1] + out_data[4 * 4 + 2];
	float blue_x = out_data[4 * 4 + 0] / blue_xyz_sum;
	float blue_y = out_data[4 * 4 + 1] / blue_xyz_sum;
	EXPECT_NEAR(0.155, blue_x, 1e-3);
	EXPECT_NEAR(0.070, blue_y, 1e-3);
	EXPECT_FLOAT_EQ(1.0f, out_data[4 * 4 + 3]);
}

TEST(ColorspaceConversionEffectTest, Rec601_625_Primaries) {
	float data[] = {
		0.0f, 0.0f, 0.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
	};
	float out_data[4 * 5];

	EffectChainTester tester(data, 1, 5, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_REC_601_625, GAMMA_LINEAR);
	tester.run(out_data, GL_RGBA, COLORSPACE_XYZ, GAMMA_LINEAR);

	// Black should stay black.
	EXPECT_FLOAT_EQ(0.0f, out_data[0 * 4 + 0]);
	EXPECT_FLOAT_EQ(0.0f, out_data[0 * 4 + 1]);
	EXPECT_FLOAT_EQ(0.0f, out_data[0 * 4 + 2]);
	EXPECT_FLOAT_EQ(1.0f, out_data[0 * 4 + 3]);

	// Convert the primaries from XYZ to xyz, and compare to the references
	// given by Rec. 601.
	float white_xyz_sum = out_data[1 * 4 + 0] + out_data[1 * 4 + 1] + out_data[1 * 4 + 2];
	float white_x = out_data[1 * 4 + 0] / white_xyz_sum;
	float white_y = out_data[1 * 4 + 1] / white_xyz_sum;
	EXPECT_NEAR(0.3127, white_x, 1e-3);
	EXPECT_NEAR(0.3290, white_y, 1e-3);
	EXPECT_FLOAT_EQ(1.0f, out_data[1 * 4 + 3]);

	float red_xyz_sum = out_data[2 * 4 + 0] + out_data[2 * 4 + 1] + out_data[2 * 4 + 2];
	float red_x = out_data[2 * 4 + 0] / red_xyz_sum;
	float red_y = out_data[2 * 4 + 1] / red_xyz_sum;
	EXPECT_NEAR(0.640, red_x, 1e-3);
	EXPECT_NEAR(0.330, red_y, 1e-3);
	EXPECT_FLOAT_EQ(1.0f, out_data[2 * 4 + 3]);

	float green_xyz_sum = out_data[3 * 4 + 0] + out_data[3 * 4 + 1] + out_data[3 * 4 + 2];
	float green_x = out_data[3 * 4 + 0] / green_xyz_sum;
	float green_y = out_data[3 * 4 + 1] / green_xyz_sum;
	EXPECT_NEAR(0.290, green_x, 1e-3);
	EXPECT_NEAR(0.600, green_y, 1e-3);
	EXPECT_FLOAT_EQ(1.0f, out_data[3 * 4 + 3]);

	float blue_xyz_sum = out_data[4 * 4 + 0] + out_data[4 * 4 + 1] + out_data[4 * 4 + 2];
	float blue_x = out_data[4 * 4 + 0] / blue_xyz_sum;
	float blue_y = out_data[4 * 4 + 1] / blue_xyz_sum;
	EXPECT_NEAR(0.150, blue_x, 1e-3);
	EXPECT_NEAR(0.060, blue_y, 1e-3);
	EXPECT_FLOAT_EQ(1.0f, out_data[4 * 4 + 3]);
}

TEST(ColorspaceConversionEffectTest, Rec2020_Primaries) {
	float data[] = {
		0.0f, 0.0f, 0.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
	};
	float out_data[4 * 5];

	EffectChainTester tester(data, 1, 5, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_REC_2020, GAMMA_LINEAR);
	tester.run(out_data, GL_RGBA, COLORSPACE_XYZ, GAMMA_LINEAR);

	// Black should stay black.
	EXPECT_FLOAT_EQ(0.0f, out_data[0 * 4 + 0]);
	EXPECT_FLOAT_EQ(0.0f, out_data[0 * 4 + 1]);
	EXPECT_FLOAT_EQ(0.0f, out_data[0 * 4 + 2]);
	EXPECT_FLOAT_EQ(1.0f, out_data[0 * 4 + 3]);

	// Convert the primaries from XYZ to xyz, and compare to the references
	// given by Rec. 2020.
	float white_xyz_sum = out_data[1 * 4 + 0] + out_data[1 * 4 + 1] + out_data[1 * 4 + 2];
	float white_x = out_data[1 * 4 + 0] / white_xyz_sum;
	float white_y = out_data[1 * 4 + 1] / white_xyz_sum;
	EXPECT_NEAR(0.3127, white_x, 1e-3);
	EXPECT_NEAR(0.3290, white_y, 1e-3);
	EXPECT_FLOAT_EQ(1.0f, out_data[1 * 4 + 3]);

	float red_xyz_sum = out_data[2 * 4 + 0] + out_data[2 * 4 + 1] + out_data[2 * 4 + 2];
	float red_x = out_data[2 * 4 + 0] / red_xyz_sum;
	float red_y = out_data[2 * 4 + 1] / red_xyz_sum;
	EXPECT_NEAR(0.708, red_x, 1e-3);
	EXPECT_NEAR(0.292, red_y, 1e-3);
	EXPECT_FLOAT_EQ(1.0f, out_data[2 * 4 + 3]);

	float green_xyz_sum = out_data[3 * 4 + 0] + out_data[3 * 4 + 1] + out_data[3 * 4 + 2];
	float green_x = out_data[3 * 4 + 0] / green_xyz_sum;
	float green_y = out_data[3 * 4 + 1] / green_xyz_sum;
	EXPECT_NEAR(0.170, green_x, 1e-3);
	EXPECT_NEAR(0.797, green_y, 1e-3);
	EXPECT_FLOAT_EQ(1.0f, out_data[3 * 4 + 3]);

	float blue_xyz_sum = out_data[4 * 4 + 0] + out_data[4 * 4 + 1] + out_data[4 * 4 + 2];
	float blue_x = out_data[4 * 4 + 0] / blue_xyz_sum;
	float blue_y = out_data[4 * 4 + 1] / blue_xyz_sum;
	EXPECT_NEAR(0.131, blue_x, 1e-3);
	EXPECT_NEAR(0.046, blue_y, 1e-3);
	EXPECT_FLOAT_EQ(1.0f, out_data[4 * 4 + 3]);
}

TEST(ColorspaceConversionEffectTest, sRGBToRec601_525) {
	float data[] = {
		0.0f, 0.0f, 0.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
		0.0f, 1.0f, 1.0f, 0.5f,
	};

	// I have to admit that most of these come from the code itself;
	// however, they do make sense if you look at the two gamuts
	// in xy space.
	float expected_data[] = {
		// Black should stay black.
		0.0f, 0.0f, 0.0f, 1.0f,

		// White should stay white (both use the D65 white point).
		1.0f, 1.0f, 1.0f, 1.0f,

		// sRGB red is slightly out-of-gamut for Rec. 601/525.
		1.064f, -0.020f, 0.0f, 1.0f,

		// Green too.
		-0.055f, 1.036f, 0.004f, 1.0f,

		// The blues are much closer; it _is_ still out-of-gamut,
		// but not actually more saturated (farther from the
		// white point).
		-0.010f, -0.017f, 0.994f, 1.0f,

		// Cyan is a mix of green and blue. Note: The alpha is kept.
		-0.065f, 1.0195f, 0.998f, 0.5f,
	};
	float out_data[4 * 6];

	EffectChainTester tester(data, 1, 6, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.run(out_data, GL_RGBA, COLORSPACE_REC_601_525, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 4, 6);
}

}  // namespace movit
