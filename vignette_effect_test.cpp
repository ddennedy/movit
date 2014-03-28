// Unit tests for VignetteEffect.

#include <epoxy/gl.h>
#include <math.h>

#include "effect_chain.h"
#include "gtest/gtest.h"
#include "image_format.h"
#include "test_util.h"
#include "vignette_effect.h"

namespace movit {

TEST(VignetteEffectTest, HugeInnerRadiusDoesNothing) {
	const int size = 4;

	float data[size * size] = {
		0.0, 1.0, 0.0, 1.0,
		0.0, 1.0, 1.0, 0.0,
		0.0, 0.5, 1.0, 0.5,
		0.0, 0.0, 0.0, 0.0,
	};
	float out_data[size * size];

	EffectChainTester tester(data, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *vignette_effect = tester.get_chain()->add_effect(new VignetteEffect());
	ASSERT_TRUE(vignette_effect->set_float("inner_radius", 10.0f));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(data, out_data, size, size);
}

TEST(VignetteEffectTest, HardCircle) {
	const int size = 16;

	float data[size * size], out_data[size * size], expected_data[size * size];
	for (int y = 0; y < size; ++y) {
		for (int x = 0; x < size; ++x) {
			data[y * size + x] = 1.0f;
		}
	}
	for (int y = 0; y < size; ++y) {
		const float yf = (y + 0.5f) / size;
		for (int x = 0; x < size; ++x) {
			const float xf = (x + 0.5f) / size;
			if (hypot(xf - 0.5, yf - 0.5) < 0.3) {
				expected_data[y * size + x] = 1.0f;
			} else {
				expected_data[y * size + x] = 0.0f;
			}
		}
	}

	EffectChainTester tester(data, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *vignette_effect = tester.get_chain()->add_effect(new VignetteEffect());
	ASSERT_TRUE(vignette_effect->set_float("radius", 0.0f));
	ASSERT_TRUE(vignette_effect->set_float("inner_radius", 0.3f));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, size, size);
}

TEST(VignetteEffectTest, BurstFromUpperLeftCorner) {
	const int width = 16, height = 24;
	float radius = 0.5f;

	float data[width * height], out_data[width * height], expected_data[width * height];
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			data[y * width + x] = 1.0f;
		}
	}
	for (int y = 0; y < height; ++y) {
		const float yf = (y + 0.5f) / width;  // Note: Division by width.
		for (int x = 0; x < width; ++x) {
			const float xf = (x + 0.5f) / width;
			const float d = hypot(xf, yf) / radius;
			if (d >= 1.0f) {
				expected_data[y * width + x] = 0.0f;
			} else {
				expected_data[y * width + x] = cos(d * 0.5 * M_PI) * cos(d * 0.5 * M_PI);
			}
		}
	}

	EffectChainTester tester(data, width, height, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *vignette_effect = tester.get_chain()->add_effect(new VignetteEffect());
	float center[] = { 0.0f, 0.0f };
	ASSERT_TRUE(vignette_effect->set_vec2("center", center));
	ASSERT_TRUE(vignette_effect->set_float("radius", radius));
	ASSERT_TRUE(vignette_effect->set_float("inner_radius", 0.0f));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, width, height);
}

}  // namespace movit
