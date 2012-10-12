// Unit tests for GammaCompressionEffect.
//
// Pretty much the inverse of the GammaExpansionEffect tests;
// EffectChainTest tests that they are actually inverses.

#include "test_util.h"
#include "gtest/gtest.h"
#include "gamma_expansion_effect.h"

TEST(GammaCompressionEffectTest, sRGB_KeyValues) {
	float data[] = {
		0.0f, 1.0f,
		0.00309f, 0.00317f,   // On either side of the discontinuity.
	};
	float expected_data[] = {
		0.0f, 1.0f,
		0.040f, 0.041f,
	};
	float out_data[4];
	EffectChainTester tester(data, 2, 2, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_sRGB);

	expect_equal(expected_data, out_data, 2, 2);
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
