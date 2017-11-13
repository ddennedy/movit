#include <string>

#include <epoxy/gl.h>
#include <assert.h>

#include "effect.h"
#include "flat_input.h"
#include "gtest/gtest.h"
#include "init.h"
#include "resource_pool.h"
#include "test_util.h"
#include "util.h"

using namespace std;

namespace movit {

// An effect that does nothing.
class IdentityComputeEffect : public Effect {
public:
	IdentityComputeEffect() {}
	virtual string effect_type_id() const { return "IdentityComputeEffect"; }
	virtual bool is_compute_shader() const { return true; }
	string output_fragment_shader() { return read_file("identity.compute"); }
};

TEST(ComputeShaderTest, Identity) {
	float data[] = {
		0.0f, 0.25f, 0.3f,
		0.75f, 1.0f, 1.0f,
	};
	float out_data[6];
	EffectChainTester tester(data, 3, 2, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	if (!movit_compute_shaders_supported) {
		fprintf(stderr, "Skipping test; no support for compile shaders.\n");
		return;
	}
	tester.get_chain()->add_effect(new IdentityComputeEffect());
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(data, out_data, 3, 2);
}

// Like IdentityComputeEffect, but due to the alpha handling, this will be
// the very last effect in the chain, which means we can't output it directly
// to the screen.
class IdentityAlphaComputeEffect : public IdentityComputeEffect {
	AlphaHandling alpha_handling() const { return DONT_CARE_ALPHA_TYPE; }
};

TEST(ComputeShaderTest, LastEffectInChain) {
	float data[] = {
		0.0f, 0.25f, 0.3f,
		0.75f, 1.0f, 1.0f,
	};
	float out_data[6];
	EffectChainTester tester(data, 3, 2, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	if (!movit_compute_shaders_supported) {
		fprintf(stderr, "Skipping test; no support for compile shaders.\n");
		return;
	}
	tester.get_chain()->add_effect(new IdentityAlphaComputeEffect());
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(data, out_data, 3, 2);
}

}  // namespace movit
