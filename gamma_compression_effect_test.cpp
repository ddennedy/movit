// Unit tests for GammaCompressionEffect.
//
// Pretty much the inverse of the GammaExpansionEffect tests;
// EffectChainTest tests that they are actually inverses.
// However, the accuracy tests are somewhat simpler, since we
// only need to care about absolute errors and not relative.

#include <epoxy/gl.h>
#include <math.h>

#include "gtest/gtest.h"
#include "gtest/gtest-message.h"
#include "image_format.h"
#include "test_util.h"

namespace movit {

TEST(GammaCompressionEffectTest, sRGB_KeyValues) {
	float data[] = {
		0.0f, 1.0f,
		0.00309f, 0.00317f,   // On either side of the discontinuity.
		-0.5f, 1.5f,          // To check clamping.
	};
	float expected_data[] = {
		0.0f, 1.0f,
		0.040f, 0.041f,
		0.0f, 1.0f,
	};
	float out_data[6];
	EffectChainTester tester(data, 2, 3, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_sRGB);

	expect_equal(expected_data, out_data, 2, 3);
}

TEST(GammaCompressionEffectTest, sRGB_RampAlwaysIncreases) {
	float data[256], out_data[256];
	for (unsigned i = 0; i < 256; ++i) {
		data[i] = i / 255.0f;
	}
	EffectChainTester tester(data, 256, 1, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_sRGB);

	for (unsigned i = 1; i < 256; ++i) {
		EXPECT_GT(out_data[i], out_data[i - 1])
		   << "No increase between " << i-1 << " and " << i;
	}
}

TEST(GammaCompressionEffectTest, sRGB_Accuracy) {
	float data[256], expected_data[256], out_data[256];

	for (int i = 0; i < 256; ++i) {
		double x = i / 255.0;

		expected_data[i] = x;
		data[i] = srgb_to_linear(x);
	}

	EffectChainTester tester(data, 256, 1, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, GL_RGBA32F);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_sRGB);

	// Maximum absolute error is 25% of one pixel level. For comparison,
	// a straightforward ALU solution (using a branch and pow()), used as a
	// “high anchor” to indicate limitations of float arithmetic etc.,
	// reaches maximum absolute error of 3.7% of one pixel level
	// and rms of 3.2e-6.
	expect_equal(expected_data, out_data, 256, 1, 0.25 / 255.0, 1e-4);
}

TEST(GammaCompressionEffectTest, Rec709_KeyValues) {
	float data[] = {
		0.0f, 1.0f,
		0.017778f, 0.018167f,  // On either side of the discontinuity.
	};
	float expected_data[] = {
		0.0f, 1.0f,
		0.080f, 0.082f,
	};
	float out_data[4];
	EffectChainTester tester(data, 2, 2, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_REC_709);

	expect_equal(expected_data, out_data, 2, 2);
}

TEST(GammaCompressionEffectTest, Rec709_RampAlwaysIncreases) {
	float data[256], out_data[256];
	for (unsigned i = 0; i < 256; ++i) {
		data[i] = i / 255.0f;
	}
	EffectChainTester tester(data, 256, 1, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_REC_709);

	for (unsigned i = 1; i < 256; ++i) {
		EXPECT_GT(out_data[i], out_data[i - 1])
		   << "No increase between " << i-1 << " and " << i;
	}
}

TEST(GammaCompressionEffectTest, Rec709_Accuracy) {
	float data[256], expected_data[256], out_data[256];

	for (int i = 0; i < 256; ++i) {
		double x = i / 255.0;

		expected_data[i] = x;

		// Rec. 2020, page 3.
		if (x < 0.018 * 4.5) {
			data[i] = x / 4.5;
		} else {
			data[i] = pow((x + 0.099) / 1.099, 1.0 / 0.45);
		}
	}

	EffectChainTester tester(data, 256, 1, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, GL_RGBA32F);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_REC_709);

	// Maximum absolute error is 25% of one pixel level. For comparison,
	// a straightforward ALU solution (using a branch and pow()), used as a
	// “high anchor” to indicate limitations of float arithmetic etc.,
	// reaches maximum absolute error of 3.7% of one pixel level
	// and rms of 3.5e-6.
	expect_equal(expected_data, out_data, 256, 1, 0.25 / 255.0, 1e-5);
}

// This test tests the same gamma ramp as Rec709_Accuracy, but with 10-bit
// input range and somewhat looser error bounds. (One could claim that this is
// already on the limit of what we can reasonably do with fp16 input, if you
// look at the local relative error.)
TEST(GammaCompressionEffectTest, Rec2020_10Bit_Accuracy) {
	float data[1024], expected_data[1024], out_data[1024];

	for (int i = 0; i < 1024; ++i) {
		double x = i / 1023.0;

		expected_data[i] = x;

		// Rec. 2020, page 3.
		if (x < 0.018 * 4.5) {
			data[i] = x / 4.5;
		} else {
			data[i] = pow((x + 0.099) / 1.099, 1.0 / 0.45);
		}
	}

	EffectChainTester tester(data, 1024, 1, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, GL_RGBA32F);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_REC_2020_10_BIT);

	// Maximum absolute error is 30% of one pixel level. For comparison,
	// a straightforward ALU solution (using a branch and pow()), used as a
	// “high anchor” to indicate limitations of float arithmetic etc.,
	// reaches maximum absolute error of 25.2% of one pixel level
	// and rms of 1.8e-6, so this is probably mostly related to input precision.
	expect_equal(expected_data, out_data, 1024, 1, 0.30 / 1023.0, 1e-5);
}

TEST(GammaCompressionEffectTest, Rec2020_12BitIsVeryCloseToRec709) {
	float data[4096];
	for (unsigned i = 0; i < 4096; ++i) {
		data[i] = i / 4095.0f;
	}
	float out_data_709[4096];
	float out_data_2020[4096];

	EffectChainTester tester(data, 4096, 1, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.run(out_data_709, GL_RED, COLORSPACE_sRGB, GAMMA_REC_709);
	EffectChainTester tester2(data, 4096, 1, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester2.run(out_data_2020, GL_RED, COLORSPACE_sRGB, GAMMA_REC_2020_12_BIT);

	double sqdiff = 0.0;
	for (unsigned i = 0; i < 4096; ++i) {
		EXPECT_NEAR(out_data_709[i], out_data_2020[i], 0.001);
		sqdiff += (out_data_709[i] - out_data_2020[i]) * (out_data_709[i] - out_data_2020[i]);
	}
	EXPECT_GT(sqdiff, 1e-6);
}

// The fp16 _input_ provided by FlatInput is not enough to distinguish between
// all of the possible 12-bit input values (every other level translates to the
// same value). Thus, this test has extremely loose bounds; if we ever decide
// to start supporting fp32, we should re-run this and tighten them a lot.
TEST(GammaCompressionEffectTest, Rec2020_12Bit_Inaccuracy) {
	float data[4096], expected_data[4096], out_data[4096];

	for (int i = 0; i < 4096; ++i) {
		double x = i / 4095.0;

		expected_data[i] = x;

		// Rec. 2020, page 3.
		if (x < 0.0181 * 4.5) {
			data[i] = x / 4.5;
		} else {
			data[i] = pow((x + 0.0993) / 1.0993, 1.0 / 0.45);
		}
	}

	EffectChainTester tester(data, 4096, 1, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, GL_RGBA32F);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_REC_2020_12_BIT);

	// Maximum absolute error is 120% of one pixel level. For comparison,
	// a straightforward ALU solution (using a branch and pow()), used as a
	// “high anchor” to indicate limitations of float arithmetic etc.,
	// reaches maximum absolute error of 71.1% of one pixel level
	// and rms of 0.9e-6, so this is probably a combination of input
	// precision and inaccuracies in the polynomial approximation.
	expect_equal(expected_data, out_data, 4096, 1, 1.2 / 4095.0, 1e-5);
}

}  // namespace movit
