// Unit tests for GammaCompressionEffect.
//
// Pretty much the inverse of the GammaExpansionEffect tests;
// EffectChainTest tests that they are actually inverses.

#include <GL/glew.h>
#include "gtest/gtest.h"
#include "image_format.h"
#include "test_util.h"

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

TEST(GammaCompressionEffectTest, Rec2020_12BitIsVeryCloseToRec709) {
	float data[256];
	for (unsigned i = 0; i < 256; ++i) {
		data[i] = i / 255.0f;
	}
	float out_data_709[256];
	float out_data_2020[256];

	EffectChainTester tester(data, 256, 1, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.run(out_data_709, GL_RED, COLORSPACE_sRGB, GAMMA_REC_709);
	EffectChainTester tester2(data, 256, 1, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester2.run(out_data_2020, GL_RED, COLORSPACE_sRGB, GAMMA_REC_2020_12_BIT);

	double sqdiff = 0.0;
	for (unsigned i = 0; i < 256; ++i) {
		EXPECT_NEAR(out_data_709[i], out_data_2020[i], 1e-3);
		sqdiff += (out_data_709[i] - out_data_2020[i]) * (out_data_709[i] - out_data_2020[i]);
	}
	EXPECT_GT(sqdiff, 1e-6);
}
