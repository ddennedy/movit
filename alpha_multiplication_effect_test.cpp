// Unit tests for AlphaMultiplicationEffect.

#include <epoxy/gl.h>

#include "effect_chain.h"
#include "gtest/gtest.h"
#include "image_format.h"
#include "test_util.h"

namespace movit {

TEST(AlphaMultiplicationEffectTest, SimpleTest) {
	const int size = 3;
	float data[4 * size] = {
		1.0f, 0.2f, 0.2f, 0.0f,
		0.2f, 1.0f, 0.2f, 0.5f,
		0.2f, 0.2f, 1.0f, 1.0f,
	};
	float expected_data[4 * size] = {
		0.0f, 0.0f, 0.0f, 0.0f,
		0.1f, 0.5f, 0.1f, 0.5f,
		0.2f, 0.2f, 1.0f, 1.0f,
	};
	float out_data[4 * size];
	EffectChainTester tester(data, 1, size, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	expect_equal(expected_data, out_data, 4, size);
}

}  // namespace movit
