// Unit tests for GammaExpansionEffect.

#include <epoxy/gl.h>
#include <math.h>

#include "gamma_expansion_effect.h"
#include "gtest/gtest.h"
#include "gtest/gtest-message.h"
#include "test_util.h"

namespace movit {

TEST(GammaExpansionEffectTest, sRGB_KeyValues) {
	float data[] = {
		0.0f, 1.0f,
		0.040f, 0.041f,  // On either side of the discontinuity.
	};
	float expected_data[] = {
		0.0f, 1.0f,
		0.00309f, 0.00317f, 
	};
	float out_data[4];
	EffectChainTester tester(data, 2, 2, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_sRGB);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 2, 2);
}

TEST(GammaExpansionEffectTest, sRGB_RampAlwaysIncreases) {
	float data[256], out_data[256];
	for (unsigned i = 0; i < 256; ++i) {
		data[i] = i / 255.0f;
	}
	EffectChainTester tester(data, 256, 1, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_sRGB);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	for (unsigned i = 1; i < 256; ++i) {
		EXPECT_GT(out_data[i], out_data[i - 1])
		   << "No increase between " << i-1 << " and " << i;
	}
}

TEST(GammaExpansionEffectTest, sRGB_AlphaIsUnchanged) {
	float data[] = {
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.25f,
		0.0f, 0.0f, 0.0f, 0.5f,
		0.0f, 0.0f, 0.0f, 0.75f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
	float out_data[5 * 4];
	EffectChainTester tester(data, 5, 1, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_sRGB);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(data, out_data, 5, 1);
}

TEST(GammaExpansionEffectTest, sRGB_Accuracy) {
	float data[256], expected_data[256], out_data[256];

	for (int i = 0; i < 256; ++i) {
		double x = i / 255.0;

		data[i] = x;
		expected_data[i] = srgb_to_linear(x);
	}

	EffectChainTester tester(data, 256, 1, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_sRGB, GL_RGBA32F);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	// Accuracy limits; for comparison, limits for a straightforward ALU solution
	// (using a branch and pow()) in parenthesis, used as a “high anchor” to
	// indicate limitations of float arithmetic etc.:
	//
	//   Maximum absolute error: 0.1% of max energy (0.051%)
	//   Maximum relative error: 2.5% of correct answer (0.093%)
	//                           25% of difference to next pixel level (6.18%)
	//   Allowed RMS error:      0.0001 (0.000010)
	//
	test_accuracy(expected_data, out_data, 256, 1e-3, 0.025, 0.25, 1e-4);
}

TEST(GammaExpansionEffectTest, Rec709_KeyValues) {
	float data[] = {
		0.0f, 1.0f,
		0.080f, 0.082f,  // On either side of the discontinuity.
	};
	float expected_data[] = {
		0.0f, 1.0f,
		0.017778f, 0.018167f,
	};
	float out_data[4];
	EffectChainTester tester(data, 2, 2, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_REC_709);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 2, 2);
}

TEST(GammaExpansionEffectTest, Rec709_RampAlwaysIncreases) {
	float data[256], out_data[256];
	for (unsigned i = 0; i < 256; ++i) {
		data[i] = i / 255.0f;
	}
	EffectChainTester tester(data, 256, 1, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_REC_709);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	for (unsigned i = 1; i < 256; ++i) {
		EXPECT_GT(out_data[i], out_data[i - 1])
		   << "No increase between " << i-1 << " and " << i;
	}
}

TEST(GammaExpansionEffectTest, Rec709_AlphaIsUnchanged) {
	float data[] = {
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.25f,
		0.0f, 0.0f, 0.0f, 0.5f,
		0.0f, 0.0f, 0.0f, 0.75f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
	float out_data[5 * 4];
	EffectChainTester tester(data, 5, 1, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_REC_709);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(data, out_data, 5, 1);
}

TEST(GammaExpansionEffectTest, Rec709_Accuracy) {
	float data[256], expected_data[256], out_data[256];

	for (int i = 0; i < 256; ++i) {
		double x = i / 255.0;

		data[i] = x;

		// Rec. 2020, page 3.
		if (x < 0.018 * 4.5) {
			expected_data[i] = x / 4.5;
		} else {
			expected_data[i] = pow((x + 0.099) / 1.099, 1.0 / 0.45);
		}
	}

	EffectChainTester tester(data, 256, 1, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_REC_709, GL_RGBA32F);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	// Accuracy limits; for comparison, limits for a straightforward ALU solution
	// (using a branch and pow()) in parenthesis, used as a “high anchor” to
	// indicate limitations of float arithmetic etc.:
	//
	//   Maximum absolute error: 0.1% of max energy (0.046%)
	//   Maximum relative error: 1.0% of correct answer (0.080%)
	//                           10% of difference to next pixel level (6.19%)
	//   Allowed RMS error:      0.0001 (0.000010)
	//
	test_accuracy(expected_data, out_data, 256, 1e-3, 0.01, 0.1, 1e-4);
}

// This test tests the same gamma ramp as Rec709_Accuracy, but with 10-bit
// input range and somewhat looser error bounds. (One could claim that this is
// already on the limit of what we can reasonably do with fp16 input, if you
// look at the local relative error.)
TEST(GammaExpansionEffectTest, Rec2020_10Bit_Accuracy) {
	float data[1024], expected_data[1024], out_data[1024];

	for (int i = 0; i < 1024; ++i) {
		double x = i / 1023.0;

		data[i] = x;

		// Rec. 2020, page 3.
		if (x < 0.018 * 4.5) {
			expected_data[i] = x / 4.5;
		} else {
			expected_data[i] = pow((x + 0.099) / 1.099, 1.0 / 0.45);
		}
	}

	EffectChainTester tester(data, 1024, 1, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_REC_2020_10_BIT, GL_RGBA32F);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	// Accuracy limits; for comparison, limits for a straightforward ALU solution
	// (using a branch and pow()) in parenthesis, used as a “high anchor” to
	// indicate limitations of float arithmetic etc.:
	//
	//   Maximum absolute error: 0.1% of max energy (0.036%)
	//   Maximum relative error: 1.0% of correct answer (0.064%)
	//                           30% of difference to next pixel level (24.9%)
	//   Allowed RMS error:      0.0001 (0.000005)
	//
	test_accuracy(expected_data, out_data, 1024, 1e-3, 0.01, 0.30, 1e-4);
}

TEST(GammaExpansionEffectTest, Rec2020_12BitIsVeryCloseToRec709) {
	float data[256];
	for (unsigned i = 0; i < 256; ++i) {
		data[i] = i / 255.0f;
	}
	float out_data_709[256];
	float out_data_2020[256];

	EffectChainTester tester(data, 256, 1, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_REC_709);
	tester.run(out_data_709, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);
	EffectChainTester tester2(data, 256, 1, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_REC_2020_12_BIT);
	tester2.run(out_data_2020, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	double sqdiff = 0.0;
	for (unsigned i = 0; i < 256; ++i) {
		EXPECT_NEAR(out_data_709[i], out_data_2020[i], 1e-3);
		sqdiff += (out_data_709[i] - out_data_2020[i]) * (out_data_709[i] - out_data_2020[i]);
	}
	EXPECT_GT(sqdiff, 1e-6);
}

// The fp16 _input_ provided by FlatInput is not enough to distinguish between
// all of the possible 12-bit input values (every other level translates to the
// same value). Thus, this test has extremely loose bounds; if we ever decide
// to start supporting fp32, we should re-run this and tighten them a lot.
TEST(GammaExpansionEffectTest, Rec2020_12Bit_Inaccuracy) {
	float data[4096], expected_data[4096], out_data[4096];

	for (int i = 0; i < 4096; ++i) {
		double x = i / 4095.0;

		data[i] = x;

		// Rec. 2020, page 3.
		if (x < 0.0181 * 4.5) {
			expected_data[i] = x / 4.5;
		} else {
			expected_data[i] = pow((x + 0.0993) / 1.0993, 1.0/0.45);
		}
	}

	EffectChainTester tester(data, 4096, 1, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_REC_2020_12_BIT, GL_RGBA32F);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	// Accuracy limits; for comparison, limits for a straightforward ALU solution
	// (using a branch and pow()) in parenthesis, used as a “high anchor” to
	// indicate limitations of float arithmetic etc.:
	//
	//   Maximum absolute error: 0.1% of max energy (0.050%)
	//   Maximum relative error: 1.0% of correct answer (0.050%)
	//                           250% of difference to next pixel level (100.00%)
	//   Allowed RMS error:      0.0001 (0.000003)
	//
	test_accuracy(expected_data, out_data, 4096, 1e-3, 0.01, 2.50, 1e-4);
}

}  // namespace movit
