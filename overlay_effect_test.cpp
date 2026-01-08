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

// Test BLEND_MODE_MULTIPLY
TEST(OverlayEffectTest, MultiplyMode) {
	float data_a[] = {
		0.5f, 0.5f, 0.5f, 1.0f
	};
	float data_b[] = {
		0.5f, 0.5f, 0.5f, 1.0f
	};
	float expected_data[] = {
		0.25f, 0.25f, 0.25f, 1.0f
	};
	float out_data[4];
	EffectChainTester tester(data_a, 1, 1, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);

	OverlayEffect *effect = new OverlayEffect();
	CHECK(effect->set_int("blend_mode", OverlayEffect::BLEND_MODE_MULTIPLY));
	tester.get_chain()->add_effect(effect, input1, input2);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	expect_equal(expected_data, out_data, 4, 1);
}

// Test BLEND_MODE_SCREEN
TEST(OverlayEffectTest, ScreenMode) {
	float data_a[] = {
		0.5f, 0.5f, 0.5f, 1.0f
	};
	float data_b[] = {
		0.5f, 0.5f, 0.5f, 1.0f
	};
	// Screen: 0.5 + 0.5 - 0.5*0.5 = 0.75
	float expected_data[] = {
		0.75f, 0.75f, 0.75f, 1.0f
	};
	float out_data[4];
	EffectChainTester tester(data_a, 1, 1, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);

	OverlayEffect *effect = new OverlayEffect();
	CHECK(effect->set_int("blend_mode", OverlayEffect::BLEND_MODE_SCREEN));
	tester.get_chain()->add_effect(effect, input1, input2);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	expect_equal(expected_data, out_data, 4, 1);
}

// Test BLEND_MODE_DARKEN
TEST(OverlayEffectTest, DarkenMode) {
	float data_a[] = {
		0.3f, 0.7f, 0.5f, 1.0f
	};
	float data_b[] = {
		0.7f, 0.3f, 0.5f, 1.0f
	};
	// Darken picks the darker of the two
	float expected_data[] = {
		0.3f, 0.3f, 0.5f, 1.0f
	};
	float out_data[4];
	EffectChainTester tester(data_a, 1, 1, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);

	OverlayEffect *effect = new OverlayEffect();
	CHECK(effect->set_int("blend_mode", OverlayEffect::BLEND_MODE_DARKEN));
	tester.get_chain()->add_effect(effect, input1, input2);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	expect_equal(expected_data, out_data, 4, 1);
}

// Test BLEND_MODE_LIGHTEN
TEST(OverlayEffectTest, LightenMode) {
	float data_a[] = {
		0.3f, 0.7f, 0.5f, 1.0f
	};
	float data_b[] = {
		0.7f, 0.3f, 0.5f, 1.0f
	};
	// Lighten picks the lighter of the two
	float expected_data[] = {
		0.7f, 0.7f, 0.5f, 1.0f
	};
	float out_data[4];
	EffectChainTester tester(data_a, 1, 1, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);

	OverlayEffect *effect = new OverlayEffect();
	CHECK(effect->set_int("blend_mode", OverlayEffect::BLEND_MODE_LIGHTEN));
	tester.get_chain()->add_effect(effect, input1, input2);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	expect_equal(expected_data, out_data, 4, 1);
}

// Test BLEND_MODE_DIFFERENCE
TEST(OverlayEffectTest, DifferenceMode) {
	float data_a[] = {
		0.7f, 0.3f, 0.5f, 1.0f
	};
	float data_b[] = {
		0.3f, 0.7f, 0.5f, 1.0f
	};
	// Difference: abs(0.7 - 0.3) = 0.4, abs(0.3 - 0.7) = 0.4, abs(0.5 - 0.5) = 0.0
	float expected_data[] = {
		0.4f, 0.4f, 0.0f, 1.0f
	};
	float out_data[4];
	EffectChainTester tester(data_a, 1, 1, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);

	OverlayEffect *effect = new OverlayEffect();
	CHECK(effect->set_int("blend_mode", OverlayEffect::BLEND_MODE_DIFFERENCE));
	tester.get_chain()->add_effect(effect, input1, input2);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	expect_equal(expected_data, out_data, 4, 1);
}

// Test BLEND_MODE_EXCLUSION
TEST(OverlayEffectTest, ExclusionMode) {
	float data_a[] = {
		0.5f, 0.5f, 0.5f, 1.0f
	};
	float data_b[] = {
		0.5f, 0.5f, 0.5f, 1.0f
	};
	// Exclusion: 0.5 + 0.5 - 2*0.5*0.5 = 1.0 - 0.5 = 0.5
	float expected_data[] = {
		0.5f, 0.5f, 0.5f, 1.0f
	};
	float out_data[4];
	EffectChainTester tester(data_a, 1, 1, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);

	OverlayEffect *effect = new OverlayEffect();
	CHECK(effect->set_int("blend_mode", OverlayEffect::BLEND_MODE_EXCLUSION));
	tester.get_chain()->add_effect(effect, input1, input2);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	expect_equal(expected_data, out_data, 4, 1);
}

// Test BLEND_MODE_PLUS
TEST(OverlayEffectTest, PlusMode) {
	float data_a[] = {
		0.3f, 0.5f, 0.7f, 0.5f
	};
	float data_b[] = {
		0.4f, 0.5f, 0.6f, 0.5f
	};
	// Plus: min(0.3 + 0.4, 1.0) = 0.7, min(0.5 + 0.5, 1.0) = 1.0, min(0.7 + 0.6, 1.0) = 1.0
	float expected_data[] = {
		0.7f, 1.0f, 1.0f, 1.0f
	};
	float out_data[4];
	EffectChainTester tester(data_a, 1, 1, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);

	OverlayEffect *effect = new OverlayEffect();
	CHECK(effect->set_int("blend_mode", OverlayEffect::BLEND_MODE_PLUS));
	tester.get_chain()->add_effect(effect, input1, input2);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	expect_equal(expected_data, out_data, 4, 1);
}

// Test BLEND_MODE_SOURCE_IN
TEST(OverlayEffectTest, SourceInMode) {
	float data_a[] = {
		0.5f, 0.5f, 0.5f, 0.8f
	};
	float data_b[] = {
		0.6f, 0.6f, 0.6f, 1.0f
	};
	// Source In (premultiplied): Sca * Da = [0.6, 0.6, 0.6] * 0.8 = [0.48, 0.48, 0.48]
	// Alpha: Sa * Da = 1.0 * 0.8 = 0.8
	float expected_data[] = {
		0.48f, 0.48f, 0.48f, 0.8f
	};
	float out_data[4];
	EffectChainTester tester(data_a, 1, 1, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);

	OverlayEffect *effect = new OverlayEffect();
	CHECK(effect->set_int("blend_mode", OverlayEffect::BLEND_MODE_SOURCE_IN));
	tester.get_chain()->add_effect(effect, input1, input2);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	expect_equal(expected_data, out_data, 4, 1);
}

// Test BLEND_MODE_SOURCE_OUT
TEST(OverlayEffectTest, SourceOutMode) {
	float data_a[] = {
		0.5f, 0.5f, 0.5f, 0.8f
	};
	float data_b[] = {
		0.6f, 0.6f, 0.6f, 1.0f
	};
	// Source Out: top * (1 - bottom.a) = 0.6 * 0.2 = 0.12
	float expected_data[] = {
		0.12f, 0.12f, 0.12f, 0.2f
	};
	float out_data[4];
	EffectChainTester tester(data_a, 1, 1, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);

	OverlayEffect *effect = new OverlayEffect();
	CHECK(effect->set_int("blend_mode", OverlayEffect::BLEND_MODE_SOURCE_OUT));
	tester.get_chain()->add_effect(effect, input1, input2);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	expect_equal(expected_data, out_data, 4, 1);
}

// Test BLEND_MODE_XOR
TEST(OverlayEffectTest, XorMode) {
	float data_a[] = {
		0.5f, 0.5f, 0.5f, 0.6f
	};
	float data_b[] = {
		0.6f, 0.6f, 0.6f, 0.8f
	};
	// Xor: top * (1 - bottom.a) + bottom * (1 - top.a)
	//    = 0.6 * 0.4 + 0.5 * 0.2 = 0.24 + 0.10 = 0.34
	// alpha = 0.6 * 0.2 + 0.8 * 0.4 = 0.12 + 0.32 = 0.44
	float expected_data[] = {
		0.34f, 0.34f, 0.34f, 0.44f
	};
	float out_data[4];
	EffectChainTester tester(data_a, 1, 1, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);

	OverlayEffect *effect = new OverlayEffect();
	CHECK(effect->set_int("blend_mode", OverlayEffect::BLEND_MODE_XOR));
	tester.get_chain()->add_effect(effect, input1, input2);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	expect_equal(expected_data, out_data, 4, 1);
}

// Test BLEND_MODE_CLEAR
TEST(OverlayEffectTest, ClearMode) {
	float data_a[] = {
		0.5f, 0.5f, 0.5f, 1.0f
	};
	float data_b[] = {
		0.6f, 0.6f, 0.6f, 1.0f
	};
	// Clear produces all zeros
	float expected_data[] = {
		0.0f, 0.0f, 0.0f, 0.0f
	};
	float out_data[4];
	EffectChainTester tester(data_a, 1, 1, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);

	OverlayEffect *effect = new OverlayEffect();
	CHECK(effect->set_int("blend_mode", OverlayEffect::BLEND_MODE_CLEAR));
	tester.get_chain()->add_effect(effect, input1, input2);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	expect_equal(expected_data, out_data, 4, 1);
}

// Test BLEND_MODE_SOURCE
TEST(OverlayEffectTest, SourceMode) {
	float data_a[] = {
		0.5f, 0.5f, 0.5f, 1.0f
	};
	float data_b[] = {
		0.6f, 0.7f, 0.8f, 1.0f
	};
	// Source just returns top
	float expected_data[] = {
		0.6f, 0.7f, 0.8f, 1.0f
	};
	float out_data[4];
	EffectChainTester tester(data_a, 1, 1, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);

	OverlayEffect *effect = new OverlayEffect();
	CHECK(effect->set_int("blend_mode", OverlayEffect::BLEND_MODE_SOURCE));
	tester.get_chain()->add_effect(effect, input1, input2);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	expect_equal(expected_data, out_data, 4, 1);
}

// Test BLEND_MODE_DESTINATION
TEST(OverlayEffectTest, DestinationMode) {
	float data_a[] = {
		0.5f, 0.6f, 0.7f, 1.0f
	};
	float data_b[] = {
		0.6f, 0.7f, 0.8f, 1.0f
	};
	// Destination just returns bottom
	float expected_data[] = {
		0.5f, 0.6f, 0.7f, 1.0f
	};
	float out_data[4];
	EffectChainTester tester(data_a, 1, 1, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);

	OverlayEffect *effect = new OverlayEffect();
	CHECK(effect->set_int("blend_mode", OverlayEffect::BLEND_MODE_DESTINATION));
	tester.get_chain()->add_effect(effect, input1, input2);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	expect_equal(expected_data, out_data, 4, 1);
}

// Test BLEND_MODE_DESTINATION_OVER
TEST(OverlayEffectTest, DestinationOverMode) {
	float data_a[] = {
		0.5f, 0.5f, 0.5f, 0.6f
	};
	float data_b[] = {
		0.3f, 0.3f, 0.3f, 0.8f
	};
	// Destination Over: bottom + (1 - bottom.a) * top
	//                 = 0.5 + 0.4 * 0.3 = 0.5 + 0.12 = 0.62
	float expected_data[] = {
		0.62f, 0.62f, 0.62f, 0.92f
	};
	float out_data[4];
	EffectChainTester tester(data_a, 1, 1, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);

	OverlayEffect *effect = new OverlayEffect();
	CHECK(effect->set_int("blend_mode", OverlayEffect::BLEND_MODE_DESTINATION_OVER));
	tester.get_chain()->add_effect(effect, input1, input2);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	expect_equal(expected_data, out_data, 4, 1);
}

// Test BLEND_MODE_DESTINATION_IN
TEST(OverlayEffectTest, DestinationInMode) {
	float data_a[] = {
		0.5f, 0.5f, 0.5f, 1.0f
	};
	float data_b[] = {
		0.6f, 0.6f, 0.6f, 0.7f
	};
	// Destination In: bottom * top.a = 0.5 * 0.7 = 0.35
	float expected_data[] = {
		0.35f, 0.35f, 0.35f, 0.7f
	};
	float out_data[4];
	EffectChainTester tester(data_a, 1, 1, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);

	OverlayEffect *effect = new OverlayEffect();
	CHECK(effect->set_int("blend_mode", OverlayEffect::BLEND_MODE_DESTINATION_IN));
	tester.get_chain()->add_effect(effect, input1, input2);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	expect_equal(expected_data, out_data, 4, 1);
}

// Test BLEND_MODE_DESTINATION_OUT
TEST(OverlayEffectTest, DestinationOutMode) {
	float data_a[] = {
		0.5f, 0.5f, 0.5f, 1.0f
	};
	float data_b[] = {
		0.6f, 0.6f, 0.6f, 0.7f
	};
	// Destination Out: bottom * (1 - top.a) = 0.5 * 0.3 = 0.15
	float expected_data[] = {
		0.15f, 0.15f, 0.15f, 0.3f
	};
	float out_data[4];
	EffectChainTester tester(data_a, 1, 1, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);

	OverlayEffect *effect = new OverlayEffect();
	CHECK(effect->set_int("blend_mode", OverlayEffect::BLEND_MODE_DESTINATION_OUT));
	tester.get_chain()->add_effect(effect, input1, input2);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	expect_equal(expected_data, out_data, 4, 1);
}

// Test BLEND_MODE_SOURCE_ATOP
TEST(OverlayEffectTest, SourceAtopMode) {
	float data_a[] = {
		0.5f, 0.5f, 0.5f, 0.6f
	};
	float data_b[] = {
		0.3f, 0.3f, 0.3f, 0.8f
	};
	// Source Atop: top * bottom.a + bottom * (1 - top.a)
	//            = 0.3 * 0.6 + 0.5 * 0.2 = 0.18 + 0.10 = 0.28
	float expected_data[] = {
		0.28f, 0.28f, 0.28f, 0.6f
	};
	float out_data[4];
	EffectChainTester tester(data_a, 1, 1, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);

	OverlayEffect *effect = new OverlayEffect();
	CHECK(effect->set_int("blend_mode", OverlayEffect::BLEND_MODE_SOURCE_ATOP));
	tester.get_chain()->add_effect(effect, input1, input2);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	expect_equal(expected_data, out_data, 4, 1);
}

// Test BLEND_MODE_DESTINATION_ATOP
TEST(OverlayEffectTest, DestinationAtopMode) {
	float data_a[] = {
		0.5f, 0.5f, 0.5f, 0.6f
	};
	float data_b[] = {
		0.3f, 0.3f, 0.3f, 0.8f
	};
	// Destination Atop: bottom * top.a + top * (1 - bottom.a)
	//                 = 0.5 * 0.8 + 0.3 * 0.4 = 0.4 + 0.12 = 0.52
	float expected_data[] = {
		0.52f, 0.52f, 0.52f, 0.8f
	};
	float out_data[4];
	EffectChainTester tester(data_a, 1, 1, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *input1 = tester.get_chain()->last_added_effect();
	Effect *input2 = tester.add_input(data_b, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);

	OverlayEffect *effect = new OverlayEffect();
	CHECK(effect->set_int("blend_mode", OverlayEffect::BLEND_MODE_DESTINATION_ATOP));
	tester.get_chain()->add_effect(effect, input1, input2);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	expect_equal(expected_data, out_data, 4, 1);
}

}  // namespace movit
