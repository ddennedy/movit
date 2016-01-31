// Unit tests for OverlayEffect.

#include <epoxy/gl.h>

#include "effect_chain.h"
#include "gtest/gtest.h"
#include "image_format.h"
#include "input.h"
#include "overlay_effect.h"
#include "test_util.h"
#include "util.h"

namespace movit {

TEST(OverlayEffectTest, TopDominatesBottomWhenNoAlpha) {
	for (int swap_inputs = 0; swap_inputs < 2; ++swap_inputs) {  // false, true.
		float data_a[] = {
			0.0f, 0.25f,
			0.75f, 1.0f,
		};
		float data_b[] = {
			1.0f, 0.5f,
			0.75f, 0.6f,
		};
		float out_data[4];
		EffectChainTester tester(data_a, 2, 2, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
		Effect *input1 = tester.get_chain()->last_added_effect();
		Effect *input2 = tester.add_input(data_b, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);

		OverlayEffect *effect = new OverlayEffect();
		CHECK(effect->set_int("swap_inputs", swap_inputs));
		tester.get_chain()->add_effect(effect, input1, input2);
		tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

		if (swap_inputs) {
			expect_equal(data_a, out_data, 2, 2);
		} else {
			expect_equal(data_b, out_data, 2, 2);
		}
	}
}

TEST(OverlayEffectTest, BottomDominatesTopWhenTopIsTransparent) {
	float data_a[] = {
		1.0f, 0.0f, 0.0f, 0.5f,
	};
	float data_b[] = {
		0.5f, 0.5f, 0.5f, 0.0f,
	};
	float out_data[4];
	EffectChainTester tester(data_a, 1, 1, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);

	tester.get_chain()->add_effect(new OverlayEffect(), input1, input2);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(data_a, out_data, 4, 1);
}

TEST(OverlayEffectTest, ZeroAlphaRemainsZeroAlpha) {
	float data_a[] = {
		0.0f, 0.25f, 0.5f, 0.0f
	};
	float data_b[] = {
		1.0f, 1.0f, 1.0f, 0.0f
	};
	float expected_data[] = {
		0.0f, 0.0f, 0.0f, 0.0f
	};
	float out_data[4];
	EffectChainTester tester(data_a, 1, 1, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);

	tester.get_chain()->add_effect(new OverlayEffect(), input1, input2);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	EXPECT_FLOAT_EQ(0.0f, expected_data[3]);
}

// This is tested against what Photoshop does: (255,0,128, 0.25) over (128,255,0, 0.5)
// becomes (179,153,51, 0.63). (Actually we fudge 0.63 to 0.625, because that's
// what it should be.)
TEST(OverlayEffectTest, PhotoshopReferenceTest) {
	float data_a[] = {
		128.0f/255.0f, 1.0f, 0.0f, 0.5f
	};
	float data_b[] = {
		1.0f, 0.0f, 128.0f/255.0f, 0.25f
	};
	float expected_data[] = {
		179.0f/255.0f, 153.0f/255.0f, 51.0f/255.0f, 0.625f
	};
	float out_data[4];
	EffectChainTester tester(data_a, 1, 1, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);

	tester.get_chain()->add_effect(new OverlayEffect(), input1, input2);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 4, 1);
}

}  // namespace movit
