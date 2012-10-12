// Unit tests for GammaExpansionEffect.

#include "test_util.h"
#include "gtest/gtest.h"
#include "gamma_expansion_effect.h"

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
	EffectChainTester tester(data, 2, 2, COLORSPACE_sRGB, GAMMA_sRGB);
	tester.run(out_data, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 2, 2);
}

TEST(GammaExpansionEffectTest, sRGB_RampAlwaysIncreases) {
	float data[256], out_data[256];
	for (unsigned i = 0; i < 256; ++i) {
		data[i] = i / 255.0f;
	}
	EffectChainTester tester(data, 256, 1, COLORSPACE_sRGB, GAMMA_sRGB);
	tester.run(out_data, COLORSPACE_sRGB, GAMMA_LINEAR);

	for (unsigned i = 1; i < 256; ++i) {
		EXPECT_GT(out_data[i], out_data[i - 1])
		   << "No increase between " << i-1 << " and " << i;
	}
}
