// Unit tests for EffectChain.
//
// Note that this also contains the tests for some of the simpler effects.

#include <GL/glew.h>

#include "effect_chain.h"
#include "flat_input.h"
#include "gtest/gtest.h"
#include "mirror_effect.h"
#include "resize_effect.h"
#include "test_util.h"

TEST(EffectChainTest, EmptyChain) {
	float data[] = {
		0.0f, 0.25f, 0.3f,
		0.75f, 1.0f, 1.0f,
	};
	float out_data[6];
	EffectChainTester tester(data, 3, 2, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

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
	EffectChainTester tester(data, 3, 2, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.get_chain()->add_effect(new IdentityEffect());
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

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
	EffectChainTester tester(data, 3, 2, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.get_chain()->add_effect(new BouncingIdentityEffect());
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

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
	EffectChainTester tester(data, 3, 2, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.get_chain()->add_effect(new MirrorEffect());
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 3, 2);
}

// A dummy effect that inverts its input.
class InvertEffect : public Effect {
public:
	InvertEffect() {}
	virtual std::string effect_type_id() const { return "InvertEffect"; }
	std::string output_fragment_shader() { return read_file("invert_effect.frag"); }
};

// Like IdentityEffect, but rewrites itself out of the loop,
// splicing in a InvertEffect instead. Also stores the new node,
// so we later can check that there are gamma conversion effects
// on both sides.
class RewritingToInvertEffect : public Effect {
public:
	RewritingToInvertEffect() {}
	virtual std::string effect_type_id() const { return "RewritingToInvertEffect"; }
	std::string output_fragment_shader() { EXPECT_TRUE(false); return read_file("identity.frag"); }
	virtual void rewrite_graph(EffectChain *graph, Node *self) {
		Node *invert_node = graph->add_node(new InvertEffect());
		graph->replace_receiver(self, invert_node);
		graph->replace_sender(self, invert_node);

		self->disabled = true;
		this->invert_node = invert_node;
	}

	Node *invert_node;	
};

TEST(EffectChainTest, RewritingWorksAndGammaConversionsAreInserted) {
	float data[] = {
		0.0f, 0.25f, 0.3f,
		0.75f, 1.0f, 1.0f,
	};
	float expected_data[6] = {
		1.0f, 0.9771f, 0.9673f,
		0.7192f, 0.0f, 0.0f,
	};
	float out_data[6];
	EffectChainTester tester(data, 3, 2, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_sRGB);
	RewritingToInvertEffect *effect = new RewritingToInvertEffect();
	tester.get_chain()->add_effect(effect);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_sRGB);

	Node *node = effect->invert_node;
	ASSERT_EQ(1, node->incoming_links.size());
	ASSERT_EQ(1, node->outgoing_links.size());
	EXPECT_EQ("GammaExpansionEffect", node->incoming_links[0]->effect->effect_type_id());
	EXPECT_EQ("GammaCompressionEffect", node->outgoing_links[0]->effect->effect_type_id());

	expect_equal(expected_data, out_data, 3, 2);
}

TEST(EffectChainTest, RewritingWorksAndTexturesAreAskedForsRGB) {
	unsigned char data[] = {
		0, 64,
		128, 255,
	};
	float expected_data[4] = {
		1.0f, 0.9771f,
		0.8983f, 0.0f,
	};
	float out_data[2];
	EffectChainTester tester(NULL, 2, 2);
	tester.add_input(data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_sRGB);
	RewritingToInvertEffect *effect = new RewritingToInvertEffect();
	tester.get_chain()->add_effect(effect);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_sRGB);

	Node *node = effect->invert_node;
	ASSERT_EQ(1, node->incoming_links.size());
	ASSERT_EQ(1, node->outgoing_links.size());
	EXPECT_EQ("FlatInput", node->incoming_links[0]->effect->effect_type_id());
	EXPECT_EQ("GammaCompressionEffect", node->outgoing_links[0]->effect->effect_type_id());

	expect_equal(expected_data, out_data, 2, 2);
}

TEST(EffectChainTest, RewritingWorksAndColorspaceConversionsAreInserted) {
	float data[] = {
		0.0f, 0.25f, 0.3f,
		0.75f, 1.0f, 1.0f,
	};
	float expected_data[6] = {
		1.0f, 0.75f, 0.7f,
		0.25f, 0.0f, 0.0f,
	};
	float out_data[6];
	EffectChainTester tester(data, 3, 2, FORMAT_GRAYSCALE, COLORSPACE_REC_601_525, GAMMA_LINEAR);
	RewritingToInvertEffect *effect = new RewritingToInvertEffect();
	tester.get_chain()->add_effect(effect);
	tester.run(out_data, GL_RED, COLORSPACE_REC_601_525, GAMMA_LINEAR);

	Node *node = effect->invert_node;
	ASSERT_EQ(1, node->incoming_links.size());
	ASSERT_EQ(1, node->outgoing_links.size());
	EXPECT_EQ("ColorspaceConversionEffect", node->incoming_links[0]->effect->effect_type_id());
	EXPECT_EQ("ColorspaceConversionEffect", node->outgoing_links[0]->effect->effect_type_id());

	expect_equal(expected_data, out_data, 3, 2);
}

// A fake input that can change its output colorspace and gamma between instantiation
// and finalize.
class UnknownColorspaceInput : public FlatInput {
public:
	UnknownColorspaceInput(ImageFormat format, MovitPixelFormat pixel_format, GLenum type, unsigned width, unsigned height)
	    : FlatInput(format, pixel_format, type, width, height),
	      overridden_color_space(format.color_space),
	      overridden_gamma_curve(format.gamma_curve) {}
	virtual std::string effect_type_id() const { return "UnknownColorspaceInput"; }

	void set_color_space(Colorspace colorspace) {
		overridden_color_space = colorspace;
	}
	void set_gamma_curve(GammaCurve gamma_curve) {
		overridden_gamma_curve = gamma_curve;
	}
	Colorspace get_color_space() const { return overridden_color_space; }
	GammaCurve get_gamma_curve() const { return overridden_gamma_curve; }

private:
	Colorspace overridden_color_space;
	GammaCurve overridden_gamma_curve;
};

TEST(EffectChainTester, HandlesInputChangingColorspace) {
	const int size = 4;

	float data[size] = {
		0.0,
		0.5,
		0.7,
		1.0,
	};
	float out_data[size];

	EffectChainTester tester(NULL, 4, 1, FORMAT_GRAYSCALE);

	// First say that we have sRGB, linear input.
	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	UnknownColorspaceInput *input = new UnknownColorspaceInput(format, FORMAT_GRAYSCALE, GL_FLOAT, 4, 1);
	input->set_pixel_data(data);
	tester.get_chain()->add_input(input);

	// Now we change to Rec. 601 input.
	input->set_color_space(COLORSPACE_REC_601_625);
	input->set_gamma_curve(GAMMA_REC_601);

	// Now ask for Rec. 601 output. Thus, our chain should now be a no-op.
	tester.run(out_data, GL_RED, COLORSPACE_REC_601_625, GAMMA_REC_601);
	expect_equal(data, out_data, 4, 1);
}

// Like RewritingToInvertEffect, but splicing in a MirrorEffect instead,
// which does not need linear light or sRGB primaries.
class RewritingToMirrorEffect : public Effect {
public:
	RewritingToMirrorEffect() {}
	virtual std::string effect_type_id() const { return "RewritingToMirrorEffect"; }
	std::string output_fragment_shader() { EXPECT_TRUE(false); return read_file("identity.frag"); }
	virtual void rewrite_graph(EffectChain *graph, Node *self) {
		Node *mirror_node = graph->add_node(new MirrorEffect());
		graph->replace_receiver(self, mirror_node);
		graph->replace_sender(self, mirror_node);

		self->disabled = true;
		this->mirror_node = mirror_node;
	}

	Node *mirror_node;
};

TEST(EffectChainTest, NoGammaConversionsWhenLinearLightNotNeeded) {
	float data[] = {
		0.0f, 0.25f, 0.3f,
		0.75f, 1.0f, 1.0f,
	};
	float expected_data[6] = {
		0.3f, 0.25f, 0.0f,
		1.0f, 1.0f, 0.75f,
	};
	float out_data[6];
	EffectChainTester tester(data, 3, 2, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_sRGB);
	RewritingToMirrorEffect *effect = new RewritingToMirrorEffect();
	tester.get_chain()->add_effect(effect);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_sRGB);

	Node *node = effect->mirror_node;
	ASSERT_EQ(1, node->incoming_links.size());
	EXPECT_EQ(0, node->outgoing_links.size());
	EXPECT_EQ("FlatInput", node->incoming_links[0]->effect->effect_type_id());

	expect_equal(expected_data, out_data, 3, 2);
}

TEST(EffectChainTest, NoColorspaceConversionsWhensRGBPrimariesNotNeeded) {
	float data[] = {
		0.0f, 0.25f, 0.3f,
		0.75f, 1.0f, 1.0f,
	};
	float expected_data[6] = {
		0.3f, 0.25f, 0.0f,
		1.0f, 1.0f, 0.75f,
	};
	float out_data[6];
	EffectChainTester tester(data, 3, 2, FORMAT_GRAYSCALE, COLORSPACE_REC_601_525, GAMMA_LINEAR);
	RewritingToMirrorEffect *effect = new RewritingToMirrorEffect();
	tester.get_chain()->add_effect(effect);
	tester.run(out_data, GL_RED, COLORSPACE_REC_601_525, GAMMA_LINEAR);

	Node *node = effect->mirror_node;
	ASSERT_EQ(1, node->incoming_links.size());
	EXPECT_EQ(0, node->outgoing_links.size());
	EXPECT_EQ("FlatInput", node->incoming_links[0]->effect->effect_type_id());

	expect_equal(expected_data, out_data, 3, 2);
}

// The identity effect needs linear light, and thus will get conversions on both sides.
// Verify that sRGB data is properly converted to and from linear light for the entire ramp.
TEST(EffectChainTest, IdentityThroughsRGBConversions) {
	float data[256];
	for (unsigned i = 0; i < 256; ++i) {
		data[i] = i / 255.0;
	};
	float out_data[256];
	EffectChainTester tester(data, 256, 1, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_sRGB);
	tester.get_chain()->add_effect(new IdentityEffect());
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_sRGB);

	expect_equal(data, out_data, 256, 1);
}

// Same, but uses the forward sRGB table from the GPU.
TEST(EffectChainTest, IdentityThroughGPUsRGBConversions) {
	unsigned char data[256];
	float expected_data[256];
	for (unsigned i = 0; i < 256; ++i) {
		data[i] = i;
		expected_data[i] = i / 255.0;
	};
	float out_data[256];
	EffectChainTester tester(NULL, 256, 1);
	tester.add_input(data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_sRGB);
	tester.get_chain()->add_effect(new IdentityEffect());
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_sRGB);

	expect_equal(expected_data, out_data, 256, 1);
}

// Same, for the Rec. 601/709 gamma curve.
TEST(EffectChainTest, IdentityThroughRec709) {
	float data[256];
	for (unsigned i = 0; i < 256; ++i) {
		data[i] = i / 255.0;
	};
	float out_data[256];
	EffectChainTester tester(data, 256, 1, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_REC_709);
	tester.get_chain()->add_effect(new IdentityEffect());
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_REC_709);

	expect_equal(data, out_data, 256, 1);
}

// Effectively scales down its input linearly by 4x (and repeating it),
// which is not attainable without mipmaps.
class MipmapNeedingEffect : public Effect {
public:
	MipmapNeedingEffect() {}
	virtual bool needs_mipmaps() const { return true; }
	virtual std::string effect_type_id() const { return "MipmapNeedingEffect"; }
	std::string output_fragment_shader() { return read_file("mipmap_needing_effect.frag"); }
	void set_gl_state(GLuint glsl_program_num, const std::string& prefix, unsigned *sampler_num)
	{
		glActiveTexture(GL_TEXTURE0);
		check_error();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		check_error();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		check_error();
	}
};

TEST(EffectChainTest, MipmapGenerationWorks) {
	float data[] = {  // In 4x4 blocks.
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,

		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,

		1.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,

		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
	};
	float expected_data[] = {  // Repeated four times each way.
		0.125f,   0.125f,   0.125f,   0.125f,
		0.09375f, 0.09375f, 0.09375f, 0.09375f,
		1.0f,     1.0f,     1.0f,     1.0f,
		0.25f,    0.25f,    0.25f,    0.25f,

		0.125f,   0.125f,   0.125f,   0.125f,
		0.09375f, 0.09375f, 0.09375f, 0.09375f,
		1.0f,     1.0f,     1.0f,     1.0f,
		0.25f,    0.25f,    0.25f,    0.25f,

		0.125f,   0.125f,   0.125f,   0.125f,
		0.09375f, 0.09375f, 0.09375f, 0.09375f,
		1.0f,     1.0f,     1.0f,     1.0f,
		0.25f,    0.25f,    0.25f,    0.25f,

		0.125f,   0.125f,   0.125f,   0.125f,
		0.09375f, 0.09375f, 0.09375f, 0.09375f,
		1.0f,     1.0f,     1.0f,     1.0f,
		0.25f,    0.25f,    0.25f,    0.25f,
	};
	float out_data[4 * 16];
	EffectChainTester tester(data, 4, 16, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.get_chain()->add_effect(new MipmapNeedingEffect());
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 4, 16);
}

TEST(EffectChainTest, ResizeDownByFourThenUpByFour) {
	float data[] = {  // In 4x4 blocks.
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,

		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,

		1.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,

		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
	};
	float expected_data[] = {  // Repeated four times horizontaly, interpolated vertically.
		0.1250f, 0.1250f, 0.1250f, 0.1250f,
		0.1250f, 0.1250f, 0.1250f, 0.1250f,
		0.1211f, 0.1211f, 0.1211f, 0.1211f,
		0.1133f, 0.1133f, 0.1133f, 0.1133f,
		0.1055f, 0.1055f, 0.1055f, 0.1055f,
		0.0977f, 0.0977f, 0.0977f, 0.0977f,
		0.2070f, 0.2070f, 0.2070f, 0.2070f,
		0.4336f, 0.4336f, 0.4336f, 0.4336f,
		0.6602f, 0.6602f, 0.6602f, 0.6602f,
		0.8867f, 0.8867f, 0.8867f, 0.8867f,
		0.9062f, 0.9062f, 0.9062f, 0.9062f,
		0.7188f, 0.7188f, 0.7188f, 0.7188f,
		0.5312f, 0.5312f, 0.5312f, 0.5312f,
		0.3438f, 0.3438f, 0.3438f, 0.3438f,
		0.2500f, 0.2500f, 0.2500f, 0.2500f,
		0.2500f, 0.2500f, 0.2500f, 0.2500f,
	};
	float out_data[4 * 16];

	ResizeEffect *downscale = new ResizeEffect();
	ASSERT_TRUE(downscale->set_int("width", 1));
	ASSERT_TRUE(downscale->set_int("height", 4));

	ResizeEffect *upscale = new ResizeEffect();
	ASSERT_TRUE(upscale->set_int("width", 4));
	ASSERT_TRUE(upscale->set_int("height", 16));

	EffectChainTester tester(data, 4, 16, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.get_chain()->add_effect(downscale);
	tester.get_chain()->add_effect(upscale);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 4, 16);
}
