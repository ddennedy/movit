// Unit tests for LiftGammaGainEffect.

#include <epoxy/gl.h>

#include "effect_chain.h"
#include "gtest/gtest.h"
#include "image_format.h"
#include "lift_gamma_gain_effect.h"
#include "test_util.h"

namespace movit {

TEST(LiftGammaGainEffectTest, DefaultIsNoop) {
	float data[] = {
		0.0f, 0.0f, 0.0f, 1.0f,
		0.5f, 0.5f, 0.5f, 0.3f,
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 0.7f,
		0.0f, 0.0f, 1.0f, 1.0f,
	};

	float out_data[5 * 4];
	EffectChainTester tester(data, 1, 5, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.get_chain()->add_effect(new LiftGammaGainEffect());
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(data, out_data, 4, 5);
}

TEST(LiftGammaGainEffectTest, Gain) {
	float data[] = {
		0.0f, 0.0f, 0.0f, 1.0f,
		0.5f, 0.5f, 0.5f, 0.3f,
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 0.7f,
		0.0f, 0.0f, 1.0f, 1.0f,
	};
	float gain[3] = { 0.8f, 1.0f, 1.2f };
	float expected_data[] = {
		0.0f, 0.0f, 0.0f, 1.0f,
		0.4f, 0.5f, 0.6f, 0.3f,
		0.8f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 0.7f,
		0.0f, 0.0f, 1.2f, 1.0f,
	};

	float out_data[5 * 4];
	EffectChainTester tester(data, 1, 5, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *lgg_effect = tester.get_chain()->add_effect(new LiftGammaGainEffect());
	ASSERT_TRUE(lgg_effect->set_vec3("gain", gain));
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 4, 5);
}

TEST(LiftGammaGainEffectTest, LiftIsDoneInApproximatelysRGB) {
	float data[] = {
		0.0f, 0.0f, 0.0f, 1.0f,
		0.5f, 0.5f, 0.5f, 0.3f,
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 0.7f,
		0.0f, 0.0f, 1.0f, 1.0f,
	};
	float lift[3] = { 0.0f, 0.1f, 0.2f };
	float expected_data[] = {
		0.0f, 0.1f , 0.2f, 1.0f,
		0.5f, 0.55f, 0.6f, 0.3f,
		1.0f, 0.1f,  0.2f, 1.0f,
		0.0f, 1.0f,  0.2f, 0.7f,
		0.0f, 0.1f,  1.0f, 1.0f,
	};

	float out_data[5 * 4];
	EffectChainTester tester(data, 1, 5, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_sRGB);
	Effect *lgg_effect = tester.get_chain()->add_effect(new LiftGammaGainEffect());
	ASSERT_TRUE(lgg_effect->set_vec3("lift", lift));
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_sRGB);

	// sRGB is only approximately gamma-2.2, so loosen up the limits a bit.
	expect_equal(expected_data, out_data, 4, 5, 0.03, 0.003);
}

TEST(LiftGammaGainEffectTest, Gamma22IsApproximatelysRGB) {
	float data[] = {
		0.0f, 0.0f, 0.0f, 1.0f,
		0.5f, 0.5f, 0.5f, 0.3f,
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 0.7f,
		0.0f, 0.0f, 1.0f, 1.0f,
	};
	float gamma[3] = { 2.2f, 2.2f, 2.2f };

	float out_data[5 * 4];
	EffectChainTester tester(data, 1, 5, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_sRGB);
	Effect *lgg_effect = tester.get_chain()->add_effect(new LiftGammaGainEffect());
	ASSERT_TRUE(lgg_effect->set_vec3("gamma", gamma));
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(data, out_data, 4, 5);
}

TEST(LiftGammaGainEffectTest, OutOfGamutColorsAreClipped) {
	float data[] = {
		-0.5f, 0.3f, 0.0f, 1.0f,
		 0.5f, 0.0f, 0.0f, 1.0f,
		 0.0f, 1.5f, 0.5f, 0.3f,
	};
	float expected_data[] = {
		 0.0f, 0.3f, 0.0f, 1.0f,  // Clipped to zero.
		 0.5f, 0.0f, 0.0f, 1.0f,
		 0.0f, 1.5f, 0.5f, 0.3f,
	};

	float out_data[3 * 4];
	EffectChainTester tester(data, 1, 3, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.get_chain()->add_effect(new LiftGammaGainEffect());
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 4, 3);
}

TEST(LiftGammaGainEffectTest, NegativeLiftIsClamped) {
	float data[] = {
		0.0f, 0.0f, 0.0f, 1.0f,
		0.5f, 0.5f, 0.5f, 0.3f,
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 0.7f,
		0.0f, 0.0f, 1.0f, 1.0f,
	};
	float lift[3] = { 0.0f, -0.1f, -0.2f };
	float expected_data[] = {
		0.0f, 0.0f , 0.0f, 1.0f,  // Note: Clamped below zero.
		0.5f, 0.45f, 0.4f, 0.3f,
		1.0f, 0.0f,  0.0f, 1.0f,  // Unaffected.
		0.0f, 1.0f,  0.0f, 0.7f,
		0.0f, 0.0f,  1.0f, 1.0f,
	};

	float out_data[5 * 4];
	EffectChainTester tester(data, 1, 5, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_sRGB);
	Effect *lgg_effect = tester.get_chain()->add_effect(new LiftGammaGainEffect());
	ASSERT_TRUE(lgg_effect->set_vec3("lift", lift));
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_sRGB);

	// sRGB is only approximately gamma-2.2, so loosen up the limits a bit.
	expect_equal(expected_data, out_data, 4, 5, 0.03, 0.003);
}

}  // namespace movit
