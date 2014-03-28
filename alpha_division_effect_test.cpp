// Unit tests for AlphaDivisionEffect.

#include <epoxy/gl.h>
#include "gtest/gtest.h"
#include "image_format.h"
#include "test_util.h"

namespace movit {

TEST(AlphaDivisionEffectTest, SimpleTest) {
	const int size = 2;
	float data[4 * size] = {
		0.1f, 0.5f, 0.1f, 0.5f,
		0.2f, 0.2f, 1.0f, 1.0f,
	};
	float expected_data[4 * size] = {
		0.2f, 1.0f, 0.2f, 0.5f,
		0.2f, 0.2f, 1.0f, 1.0f,
	};
	float out_data[4 * size];
	EffectChainTester tester(data, 1, size, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 4, size);
}

TEST(AlphaDivisionEffectTest, ZeroAlphaIsPreserved) {
	const int size = 2;
	float data[4 * size] = {
		0.1f, 0.5f, 0.1f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
	};
	float out_data[4 * size];
	EffectChainTester tester(data, 1, size, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	EXPECT_EQ(0.0f, out_data[3]);
	EXPECT_EQ(0.0f, out_data[7]);
}

}  // namespace movit
