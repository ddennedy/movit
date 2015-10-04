// Unit tests for DitherEffect.
//
// Note: Dithering of multiple outputs is tested (somewhat weakly)
// in YCbCrConversionEffectTest.

#include <epoxy/gl.h>
#include <math.h>

#include "effect_chain.h"
#include "gtest/gtest.h"
#include "image_format.h"
#include "test_util.h"
#include "util.h"

namespace movit {

TEST(DitherEffectTest, NoDitherOnExactValues) {
	const int size = 4;

	float data[size * size] = {
		0.0, 1.0, 0.0, 1.0,
		0.0, 1.0, 1.0, 0.0,
		0.0, 0.2, 1.0, 0.2,
		0.0, 0.0, 0.0, 0.0,
	};
	unsigned char expected_data[size * size] = {
		0, 255,   0, 255,
		0, 255, 255,   0,
		0,  51, 255,  51,
		0,   0,   0,   0,
	};
	unsigned char out_data[size * size];

	EffectChainTester tester(data, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, GL_RGBA8);
	check_error();
	tester.get_chain()->set_dither_bits(8);
	check_error();
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);
	check_error();

	expect_equal(expected_data, out_data, size, size);
}

TEST(DitherEffectTest, SinusoidBelowOneLevelComesThrough) {
	const float frequency = 0.3f * M_PI;
	const unsigned size = 2048;
	const float amplitude = 0.25f / 255.0f;  // 6 dB below what can be represented without dithering.

	float data[size];
	for (unsigned i = 0; i < size; ++i) {
		data[i] = 0.2 + amplitude * sin(i * frequency);
	}
	unsigned char out_data[size];

	EffectChainTester tester(data, size, 1, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, GL_RGBA8);
	tester.get_chain()->set_dither_bits(8);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	// Measure how strong the given sinusoid is in the output.
	float sum = 0.0f;	
	for (unsigned i = 0; i < size; ++i) {
		sum += 2.0 * (int(out_data[i]) - 0.2*255.0) * sin(i * frequency);
	}

	EXPECT_NEAR(amplitude, sum / (size * 255.0f), 1.1e-5);
}

}  // namespace movit
