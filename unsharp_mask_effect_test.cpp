// Unit tests for UnsharpMaskEffect.

#include <epoxy/gl.h>
#include <math.h>

#include "effect_chain.h"
#include "gtest/gtest.h"
#include "image_format.h"
#include "test_util.h"
#include "unsharp_mask_effect.h"

namespace movit {

TEST(UnsharpMaskEffectTest, NoAmountDoesNothing) {
	const int size = 4;

	float data[size * size] = {
		0.0, 1.0, 0.0, 1.0,
		0.0, 1.0, 1.0, 0.0,
		0.0, 0.5, 1.0, 0.5,
		0.0, 0.0, 0.0, 0.0,
	};
	float out_data[size * size];

	EffectChainTester tester(data, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *unsharp_mask_effect = tester.get_chain()->add_effect(new UnsharpMaskEffect());
	ASSERT_TRUE(unsharp_mask_effect->set_float("radius", 2.0f));
	ASSERT_TRUE(unsharp_mask_effect->set_float("amount", 0.0f));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(data, out_data, size, size);
}

TEST(UnsharpMaskEffectTest, UnblursGaussianBlur) {
	const int size = 13;
	const float sigma = 0.5f;

	float data[size * size], out_data[size * size];
	float expected_data[] = {  // One single dot in the middle.
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 
		0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 
	};

	// Create a Gaussian input. (Note that our blur is not Gaussian.)
	for (int y = 0; y < size; ++y) {
		for (int x = 0; x < size; ++x) {
			float z = hypot(x - 6, y - 6);
			data[y * size + x] = exp(-z*z / (2.0 * sigma * sigma)) / (2.0 * M_PI * sigma * sigma);
		}
	}

	EffectChainTester tester(data, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *unsharp_mask_effect = tester.get_chain()->add_effect(new UnsharpMaskEffect());
	ASSERT_TRUE(unsharp_mask_effect->set_float("radius", sigma));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	// Check the center sample separately; it's bound to be the sample with the largest
	// single error, and we know we can't get it perfect anyway.
	int center = size / 2;
	EXPECT_GT(out_data[center * size + center], 0.45f);
	out_data[center * size + center] = 1.0f;

	// Add some leeway for the rest; unsharp masking is not expected to be extremely good.
	expect_equal(expected_data, out_data, size, size, 0.1, 0.001);
}

}  // namespace movit
