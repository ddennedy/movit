// Unit tests for EffectChain.
//
// Note that this also contains the tests for some of the simpler effects.

#include "effect_chain.h"
#include "flat_input.h"
#include "mirror_effect.h"
#include "opengl.h"
#include "gtest/gtest.h"

#include <stdio.h>
#include <math.h>

#include <algorithm>

class EffectChainTester {
public:
	EffectChainTester(const float *data, unsigned width, unsigned height, ColorSpace color_space, GammaCurve gamma_curve)
		: chain(width, height), width(width), height(height)
	{
		ImageFormat format;
		format.color_space = color_space;
		format.gamma_curve = gamma_curve;
	
		FlatInput *input = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, width, height);
		input->set_pixel_data(data);
		chain.add_input(input);
	}

	EffectChain *get_chain() { return &chain; }

	void run(float *out_data, ColorSpace color_space, GammaCurve gamma_curve)
	{
		ImageFormat format;
		format.color_space = color_space;
		format.gamma_curve = gamma_curve;
		chain.add_output(format);
		chain.finalize();

		glViewport(0, 0, width, height);
		chain.render_to_screen();

		glReadPixels(0, 0, width, height, GL_RED, GL_FLOAT, out_data);

		// Flip upside-down to compensate for different origin.
		for (unsigned y = 0; y < height / 2; ++y) {
			unsigned flip_y = height - y - 1;
			for (unsigned x = 0; x < width; ++x) {
				std::swap(out_data[y * width + x], out_data[flip_y * width + x]);
			}
		}
	}

private:
	EffectChain chain;
	unsigned width, height;
};

void expect_equal(const float *ref, const float *result, unsigned width, unsigned height)
{
	float largest_difference = -1.0f;
	float squared_difference = 0.0f;

	for (unsigned y = 0; y < height; ++y) {
		for (unsigned x = 0; x < width; ++x) {
			float diff = ref[y * width + x] - result[y * width + x];
			largest_difference = std::max(largest_difference, fabsf(diff));
			squared_difference += diff * diff;
		}
	}

	const float largest_difference_limit = 1.5 / 255.0;
	const float rms_limit = 0.5 / 255.0;

	EXPECT_LT(largest_difference, largest_difference_limit);

	float rms = sqrt(squared_difference) / (width * height);
	EXPECT_LT(rms, rms_limit);

	if (largest_difference >= largest_difference_limit || rms >= rms_limit) {
		fprintf(stderr, "Dumping matrices for easier debugging, since at least one test failed.\n");

		fprintf(stderr, "Reference:\n");
		for (unsigned y = 0; y < height; ++y) {
			for (unsigned x = 0; x < width; ++x) {
				fprintf(stderr, "%7.4f ", ref[y * width + x]);
			}
			fprintf(stderr, "\n");
		}

		fprintf(stderr, "\nResult:\n");
		for (unsigned y = 0; y < height; ++y) {
			for (unsigned x = 0; x < width; ++x) {
				fprintf(stderr, "%7.4f ", result[y * width + x]);
			}
			fprintf(stderr, "\n");
		}
	}
}

TEST(EffectChainTest, EmptyChain) {
	float data[] = {
		0.0f, 0.25f, 0.3f,
		0.75f, 1.0f, 1.0f,
	};
	float out_data[6];
	EffectChainTester tester(data, 3, 2, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.run(out_data, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(data, out_data, 3, 2);
}

// An effect that does nothing.
class IdentityEffect : public Effect {
public:
	IdentityEffect() {}
	virtual std::string effect_type_id() const { return "IdentityEffect"; }
	std::string output_fragment_shader() { return read_file("identity.frag"); }
};

TEST(EffectChainTest, Identity) {
	float data[] = {
		0.0f, 0.25f, 0.3f,
		0.75f, 1.0f, 1.0f,
	};
	float out_data[6];
	EffectChainTester tester(data, 3, 2, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.get_chain()->add_effect(new IdentityEffect());
	tester.run(out_data, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(data, out_data, 3, 2);
}

// An effect that does nothing, but requests texture bounce.
class BouncingIdentityEffect : public Effect {
public:
	BouncingIdentityEffect() {}
	virtual std::string effect_type_id() const { return "IdentityEffect"; }
	std::string output_fragment_shader() { return read_file("identity.frag"); }
	bool needs_texture_bounce() const { return true; }
};

TEST(EffectChainTest, TextureBouncePreservesIdentity) {
	float data[] = {
		0.0f, 0.25f, 0.3f,
		0.75f, 1.0f, 1.0f,
	};
	float out_data[6];
	EffectChainTester tester(data, 3, 2, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.get_chain()->add_effect(new BouncingIdentityEffect());
	tester.run(out_data, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(data, out_data, 3, 2);
}

TEST(MirrorTest, BasicTest) {
	float data[] = {
		0.0f, 0.25f, 0.3f,
		0.75f, 1.0f, 1.0f,
	};
	float expected_data[6] = {
		0.3f, 0.25f, 0.0f,
		1.0f, 1.0f, 0.75f,
	};
	float out_data[6];
	EffectChainTester tester(data, 3, 2, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.get_chain()->add_effect(new MirrorEffect());
	tester.run(out_data, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 3, 2);
}
