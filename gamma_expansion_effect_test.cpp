// Unit tests for GammaExpansionEffect.

#include <GL/glew.h>

#include "gamma_expansion_effect.h"
#include "gtest/gtest.h"
#include "test_util.h"

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
