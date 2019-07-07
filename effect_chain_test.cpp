// Unit tests for EffectChain.
//
// Note that this also contains the tests for some of the simpler effects.

#include <locale>
#include <sstream>
#include <string>

#include <epoxy/gl.h>
#include <assert.h>

#include "effect.h"
#include "effect_chain.h"
#include "flat_input.h"
#include "gtest/gtest.h"
#include "init.h"
#include "input.h"
#include "mirror_effect.h"
#include "multiply_effect.h"
#include "resize_effect.h"
#include "resource_pool.h"
#include "test_util.h"
#include "util.h"

using namespace std;

namespace movit {

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
	string effect_type_id() const override { return "IdentityEffect"; }
	string output_fragment_shader() override { return read_file("identity.frag"); }
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
	string effect_type_id() const override { return "IdentityEffect"; }
	string output_fragment_shader() override { return read_file("identity.frag"); }
	bool needs_texture_bounce() const override { return true; }
	AlphaHandling alpha_handling() const override { return DONT_CARE_ALPHA_TYPE; }
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

class WithAndWithoutComputeShaderTest : public testing::TestWithParam<string> {
};
INSTANTIATE_TEST_CASE_P(WithAndWithoutComputeShaderTest,
                        WithAndWithoutComputeShaderTest,
                        testing::Values("fragment", "compute"));

// An effect that does nothing, but as a compute shader.
class IdentityComputeEffect : public Effect {
public:
	IdentityComputeEffect() {}
	virtual string effect_type_id() const { return "IdentityComputeEffect"; }
	virtual bool is_compute_shader() const { return true; }
	string output_fragment_shader() { return read_file("identity.comp"); }
};

TEST_P(WithAndWithoutComputeShaderTest, TopLeftOrigin) {
	float data[] = {
		0.0f, 0.25f, 0.3f,
		0.75f, 1.0f, 1.0f,
	};
	// Note that EffectChainTester assumes bottom-left origin, so by setting
	// top-left, we will get flipped data back.
	float expected_data[6] = {
		0.75f, 1.0f, 1.0f,
		0.0f, 0.25f, 0.3f,
	};
	float out_data[6];
	EffectChainTester tester(data, 3, 2, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.get_chain()->set_output_origin(OUTPUT_ORIGIN_TOP_LEFT);
	if (GetParam() == "compute") {
		if (!movit_compute_shaders_supported) {
			fprintf(stderr, "Skipping test; no support for compile shaders.\n");
			return;
		}
		tester.get_chain()->add_effect(new IdentityComputeEffect());
	}
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 3, 2);
}

// A dummy effect that inverts its input.
class InvertEffect : public Effect {
public:
	InvertEffect() {}
	string effect_type_id() const override { return "InvertEffect"; }
	string output_fragment_shader() override { return read_file("invert_effect.frag"); }

	// A real invert would actually care about its alpha,
	// but in this unit test, it only complicates things.
	AlphaHandling alpha_handling() const override { return DONT_CARE_ALPHA_TYPE; }
};

// Like IdentityEffect, but rewrites itself out of the loop,
// splicing in a different effect instead. Also stores the new node,
// so we later can check whatever properties we'd like about the graph.
template<class T>
class RewritingEffect : public Effect {
public:
	template<class... Args>
	RewritingEffect(Args &&... args) : effect(new T(std::forward<Args>(args)...)), replaced_node(nullptr) {}
	string effect_type_id() const override { return "RewritingEffect[" + effect->effect_type_id() + "]"; }
	string output_fragment_shader() override { EXPECT_TRUE(false); return read_file("identity.frag"); }
	void rewrite_graph(EffectChain *graph, Node *self) override {
		replaced_node = graph->add_node(effect);
		graph->replace_receiver(self, replaced_node);
		graph->replace_sender(self, replaced_node);
		self->disabled = true;
	}

	T *effect;
	Node *replaced_node;
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
	RewritingEffect<InvertEffect> *effect = new RewritingEffect<InvertEffect>();
	tester.get_chain()->add_effect(effect);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_sRGB);

	Node *node = effect->replaced_node;
	ASSERT_EQ(1u, node->incoming_links.size());
	ASSERT_EQ(1u, node->outgoing_links.size());
	EXPECT_EQ("GammaExpansionEffect", node->incoming_links[0]->effect->effect_type_id());
	EXPECT_EQ("GammaCompressionEffect", node->outgoing_links[0]->effect->effect_type_id());

	expect_equal(expected_data, out_data, 3, 2);
}

TEST(EffectChainTest, RewritingWorksAndTexturesAreAskedForsRGB) {
	unsigned char data[] = {
		  0,   0,   0, 255,
		 64,  64,  64, 255,
		128, 128, 128, 255,
		255, 255, 255, 255,
	};
	float expected_data[] = {
		1.0000f, 1.0000f, 1.0000f, 1.0000f,
		0.9771f, 0.9771f, 0.9771f, 1.0000f,
		0.8983f, 0.8983f, 0.8983f, 1.0000f,
		0.0000f, 0.0000f, 0.0000f, 1.0000f
	};
	float out_data[4 * 4];
	EffectChainTester tester(nullptr, 1, 4);
	tester.add_input(data, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_sRGB);
	RewritingEffect<InvertEffect> *effect = new RewritingEffect<InvertEffect>();
	tester.get_chain()->add_effect(effect);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_sRGB);

	Node *node = effect->replaced_node;
	ASSERT_EQ(1u, node->incoming_links.size());
	ASSERT_EQ(1u, node->outgoing_links.size());
	EXPECT_EQ("FlatInput", node->incoming_links[0]->effect->effect_type_id());
	EXPECT_EQ("GammaCompressionEffect", node->outgoing_links[0]->effect->effect_type_id());

	expect_equal(expected_data, out_data, 4, 4);
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
	RewritingEffect<InvertEffect> *effect = new RewritingEffect<InvertEffect>();
	tester.get_chain()->add_effect(effect);
	tester.run(out_data, GL_RED, COLORSPACE_REC_601_525, GAMMA_LINEAR);

	Node *node = effect->replaced_node;
	ASSERT_EQ(1u, node->incoming_links.size());
	ASSERT_EQ(1u, node->outgoing_links.size());
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
	string effect_type_id() const override { return "UnknownColorspaceInput"; }

	void set_color_space(Colorspace colorspace) {
		overridden_color_space = colorspace;
	}
	void set_gamma_curve(GammaCurve gamma_curve) {
		overridden_gamma_curve = gamma_curve;
	}
	Colorspace get_color_space() const override { return overridden_color_space; }
	GammaCurve get_gamma_curve() const override { return overridden_gamma_curve; }

private:
	Colorspace overridden_color_space;
	GammaCurve overridden_gamma_curve;
};

TEST(EffectChainTest, HandlesInputChangingColorspace) {
	const int size = 4;

	float data[size] = {
		0.0,
		0.5,
		0.7,
		1.0,
	};
	float out_data[size];

	EffectChainTester tester(nullptr, 4, 1, FORMAT_GRAYSCALE);

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
	RewritingEffect<MirrorEffect> *effect = new RewritingEffect<MirrorEffect>();
	tester.get_chain()->add_effect(effect);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_sRGB);

	Node *node = effect->replaced_node;
	ASSERT_EQ(1u, node->incoming_links.size());
	EXPECT_EQ(0u, node->outgoing_links.size());
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
	RewritingEffect<MirrorEffect> *effect = new RewritingEffect<MirrorEffect>();
	tester.get_chain()->add_effect(effect);
	tester.run(out_data, GL_RED, COLORSPACE_REC_601_525, GAMMA_LINEAR);

	Node *node = effect->replaced_node;
	ASSERT_EQ(1u, node->incoming_links.size());
	EXPECT_EQ(0u, node->outgoing_links.size());
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
	EffectChainTester tester(nullptr, 256, 1);
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

// The identity effect needs premultiplied alpha, and thus will get conversions on both sides.
TEST(EffectChainTest, IdentityThroughAlphaConversions) {
	const int size = 3;
	float data[4 * size] = {
		0.8f, 0.0f, 0.0f, 0.5f,
		0.0f, 0.2f, 0.2f, 0.3f,
		0.1f, 0.0f, 1.0f, 1.0f,
	};
	float out_data[4 * size];
	EffectChainTester tester(data, size, 1, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.get_chain()->add_effect(new IdentityEffect());
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(data, out_data, 4, size);
}

TEST(EffectChainTest, NoAlphaConversionsWhenPremultipliedAlphaNotNeeded) {
	const int size = 3;
	float data[4 * size] = {
		0.8f, 0.0f, 0.0f, 0.5f,
		0.0f, 0.2f, 0.2f, 0.3f,
		0.1f, 0.0f, 1.0f, 1.0f,
	};
	float expected_data[4 * size] = {
		0.1f, 0.0f, 1.0f, 1.0f,
		0.0f, 0.2f, 0.2f, 0.3f,
		0.8f, 0.0f, 0.0f, 0.5f,
	};
	float out_data[4 * size];
	EffectChainTester tester(data, size, 1, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	RewritingEffect<MirrorEffect> *effect = new RewritingEffect<MirrorEffect>();
	tester.get_chain()->add_effect(effect);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	Node *node = effect->replaced_node;
	ASSERT_EQ(1u, node->incoming_links.size());
	EXPECT_EQ(0u, node->outgoing_links.size());
	EXPECT_EQ("FlatInput", node->incoming_links[0]->effect->effect_type_id());

	expect_equal(expected_data, out_data, 4, size);
}

// An input that outputs only blue, which has blank alpha.
class BlueInput : public Input {
public:
	BlueInput() { register_int("needs_mipmaps", &needs_mipmaps); }
	string effect_type_id() const override { return "IdentityEffect"; }
	string output_fragment_shader() override { return read_file("blue.frag"); }
	AlphaHandling alpha_handling() const override { return OUTPUT_BLANK_ALPHA; }
	bool can_output_linear_gamma() const override { return true; }
	unsigned get_width() const override { return 1; }
	unsigned get_height() const override { return 1; }
	Colorspace get_color_space() const override { return COLORSPACE_sRGB; }
	GammaCurve get_gamma_curve() const override { return GAMMA_LINEAR; }

private:
	int needs_mipmaps;
};

// Like RewritingEffect<InvertEffect>, but splicing in a BlueInput instead,
// which outputs blank alpha.
class RewritingToBlueInput : public Input {
public:
	RewritingToBlueInput() : blue_node(nullptr) { register_int("needs_mipmaps", &needs_mipmaps); }
	string effect_type_id() const override { return "RewritingToBlueInput"; }
	string output_fragment_shader() override { EXPECT_TRUE(false); return read_file("identity.frag"); }
	void rewrite_graph(EffectChain *graph, Node *self) override {
		Node *blue_node = graph->add_node(new BlueInput());
		graph->replace_receiver(self, blue_node);
		graph->replace_sender(self, blue_node);

		self->disabled = true;
		this->blue_node = blue_node;
	}

	// Dummy values that we need to implement because we inherit from Input.
	// Same as BlueInput.
	AlphaHandling alpha_handling() const override { return OUTPUT_BLANK_ALPHA; }
	bool can_output_linear_gamma() const override { return true; }
	unsigned get_width() const override { return 1; }
	unsigned get_height() const override { return 1; }
	Colorspace get_color_space() const override { return COLORSPACE_sRGB; }
	GammaCurve get_gamma_curve() const override { return GAMMA_LINEAR; }

	Node *blue_node;

private:
	int needs_mipmaps;
};

TEST(EffectChainTest, NoAlphaConversionsWithBlankAlpha) {
	const int size = 3;
	float data[4 * size] = {
		0.0f, 0.0f, 1.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
	};
	float out_data[4 * size];
	EffectChainTester tester(nullptr, size, 1);
	RewritingToBlueInput *input = new RewritingToBlueInput();
	tester.get_chain()->add_input(input);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	Node *node = input->blue_node;
	EXPECT_EQ(0u, node->incoming_links.size());
	EXPECT_EQ(0u, node->outgoing_links.size());

	expect_equal(data, out_data, 4, size);
}

// An effect that does nothing, and specifies that it preserves blank alpha.
class BlankAlphaPreservingEffect : public Effect {
public:
	BlankAlphaPreservingEffect() {}
	string effect_type_id() const override { return "BlankAlphaPreservingEffect"; }
	string output_fragment_shader() override { return read_file("identity.frag"); }
	AlphaHandling alpha_handling() const override { return INPUT_PREMULTIPLIED_ALPHA_KEEP_BLANK; }
};

TEST(EffectChainTest, NoAlphaConversionsWithBlankAlphaPreservingEffect) {
	const int size = 3;
	float data[4 * size] = {
		0.0f, 0.0f, 1.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
	};
	float out_data[4 * size];
	EffectChainTester tester(nullptr, size, 1);
	tester.get_chain()->add_input(new BlueInput());
	tester.get_chain()->add_effect(new BlankAlphaPreservingEffect());
	RewritingEffect<MirrorEffect> *effect = new RewritingEffect<MirrorEffect>();
	tester.get_chain()->add_effect(effect);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);

	Node *node = effect->replaced_node;
	EXPECT_EQ(1u, node->incoming_links.size());
	EXPECT_EQ(0u, node->outgoing_links.size());

	expect_equal(data, out_data, 4, size);
}

// This is the counter-test to NoAlphaConversionsWithBlankAlphaPreservingEffect;
// just to be sure that with a normal INPUT_AND_OUTPUT_PREMULTIPLIED_ALPHA effect,
// an alpha conversion _should_ be inserted at the very end. (There is some overlap
// with other tests.)
TEST(EffectChainTest, AlphaConversionsWithNonBlankAlphaPreservingEffect) {
	const int size = 3;
	float data[4 * size] = {
		0.0f, 0.0f, 1.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
	};
	float out_data[4 * size];
	EffectChainTester tester(nullptr, size, 1);
	tester.get_chain()->add_input(new BlueInput());
	tester.get_chain()->add_effect(new IdentityEffect());  // Not BlankAlphaPreservingEffect.
	RewritingEffect<MirrorEffect> *effect = new RewritingEffect<MirrorEffect>();
	tester.get_chain()->add_effect(effect);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);

	Node *node = effect->replaced_node;
	EXPECT_EQ(1u, node->incoming_links.size());
	EXPECT_EQ(1u, node->outgoing_links.size());
	EXPECT_EQ("AlphaDivisionEffect", node->outgoing_links[0]->effect->effect_type_id());

	expect_equal(data, out_data, 4, size);
}

// Effectively scales down its input linearly by 4x (and repeating it),
// which is not attainable without mipmaps.
class MipmapNeedingEffect : public Effect {
public:
	MipmapNeedingEffect() {}
	MipmapRequirements needs_mipmaps() const override { return NEEDS_MIPMAPS; }

	// To be allowed to mess with the sampler state.
	bool needs_texture_bounce() const override { return true; }

	string effect_type_id() const override { return "MipmapNeedingEffect"; }
	string output_fragment_shader() override { return read_file("mipmap_needing_effect.frag"); }
	void inform_added(EffectChain *chain) override { this->chain = chain; }

	void set_gl_state(GLuint glsl_program_num, const string& prefix, unsigned *sampler_num) override
	{
		Node *self = chain->find_node_for_effect(this);
		glActiveTexture(chain->get_input_sampler(self, 0));
		check_error();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		check_error();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		check_error();
	}

private:
	EffectChain *chain;
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

class NonMipmapCapableInput : public FlatInput {
public:
	NonMipmapCapableInput(ImageFormat format, MovitPixelFormat pixel_format, GLenum type, unsigned width, unsigned height)
		: FlatInput(format, pixel_format, type, width, height) {}

	bool can_supply_mipmaps() const override { return false; }
	bool set_int(const std::string& key, int value) override {
		if (key == "needs_mipmaps") {
			assert(value == 0);
		}
		return FlatInput::set_int(key, value);
	}
};

// The same test as MipmapGenerationWorks, but with an input that refuses
// to supply mipmaps.
TEST(EffectChainTest, MipmapsWithNonMipmapCapableInput) {
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
	EffectChainTester tester(nullptr, 4, 16, FORMAT_GRAYSCALE);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	NonMipmapCapableInput *input = new NonMipmapCapableInput(format, FORMAT_GRAYSCALE, GL_FLOAT, 4, 16);
	input->set_pixel_data(data);
	tester.get_chain()->add_input(input);
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

// An effect to verify that you can turn off mipmaps; it downscales by two,
// which gives blur with mipmaps and aliasing (picks out every other pixel)
// without.
class Downscale2xEffect : public Effect {
public:
	explicit Downscale2xEffect(MipmapRequirements mipmap_requirements)
		: mipmap_requirements(mipmap_requirements)
	{
		register_vec2("offset", offset);
	}
	MipmapRequirements needs_mipmaps() const override { return mipmap_requirements; }

	string effect_type_id() const override { return "Downscale2xEffect"; }
	string output_fragment_shader() override { return read_file("downscale2x.frag"); }

private:
	const MipmapRequirements mipmap_requirements;
	float offset[2] { 0.0f, 0.0f };
};

TEST(EffectChainTest, MipmapChainGetsSplit) {
	float data[] = {
		0.0f, 0.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 1.0f, 0.0f,
	};

	// The intermediate result after the first step looks like this,
	// assuming there are no mipmaps (the zeros are due to border behavior):
	//
	//   0 0 0 0
	//   0 0 0 0
	//   1 1 0 0
	//   1 1 0 0
	//
	// so another 2x downscale towards the bottom left will give
	//
	//   0 0
	//   1 0
	//
	// with yet more zeros coming in on the top and the right from the border.
	float expected_data[] = {
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f, 0.0f,
	};
	float out_data[4 * 4];

	float offset[] = { -0.5f / 4.0f, -0.5f / 4.0f };
	RewritingEffect<Downscale2xEffect> *pick_out_bottom_left = new RewritingEffect<Downscale2xEffect>(Effect::CANNOT_ACCEPT_MIPMAPS);
	ASSERT_TRUE(pick_out_bottom_left->effect->set_vec2("offset", offset));

	RewritingEffect<Downscale2xEffect> *downscale2x = new RewritingEffect<Downscale2xEffect>(Effect::NEEDS_MIPMAPS);

	EffectChainTester tester(data, 4, 4, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.get_chain()->add_effect(pick_out_bottom_left);
	tester.get_chain()->add_effect(downscale2x);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	EXPECT_NE(pick_out_bottom_left->replaced_node->containing_phase,
	          downscale2x->replaced_node->containing_phase);

	expect_equal(expected_data, out_data, 4, 4);
}

// An effect that adds its two inputs together. Used below.
class AddEffect : public Effect {
public:
	AddEffect() {}
	string effect_type_id() const override { return "AddEffect"; }
	string output_fragment_shader() override { return read_file("add.frag"); }
	unsigned num_inputs() const override { return 2; }
	AlphaHandling alpha_handling() const override { return DONT_CARE_ALPHA_TYPE; }
};

// Constructs the graph
//
//             FlatInput               |
//            /         \              |
//  MultiplyEffect  MultiplyEffect     |
//            \         /              |
//             AddEffect               |
//
// and verifies that it gives the correct output.
TEST(EffectChainTest, DiamondGraph) {
	float data[] = {
		1.0f, 1.0f,
		1.0f, 0.0f,
	};
	float expected_data[] = {
		2.5f, 2.5f,
		2.5f, 0.0f,
	};
	float out_data[2 * 2];

	const float half[] = { 0.5f, 0.5f, 0.5f, 0.5f };
	const float two[] = { 2.0f, 2.0f, 2.0f, 0.5f };

	MultiplyEffect *mul_half = new MultiplyEffect();
	ASSERT_TRUE(mul_half->set_vec4("factor", half));
	
	MultiplyEffect *mul_two = new MultiplyEffect();
	ASSERT_TRUE(mul_two->set_vec4("factor", two));

	EffectChainTester tester(nullptr, 2, 2);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	FlatInput *input = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, 2, 2);
	input->set_pixel_data(data);

	tester.get_chain()->add_input(input);
	tester.get_chain()->add_effect(mul_half, input);
	tester.get_chain()->add_effect(mul_two, input);
	tester.get_chain()->add_effect(new AddEffect(), mul_half, mul_two);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 2, 2);
}

// Constructs the graph
//
//             FlatInput                     |
//            /         \                    |
//  MultiplyEffect  MultiplyEffect           |
//         \             |                   |
//          \    BouncingIdentityEffect      |  
//            \         /                    |
//             AddEffect                     |
//
// and verifies that it gives the correct output.
TEST(EffectChainTest, DiamondGraphWithOneInputUsedInTwoPhases) {
	float data[] = {
		1.0f, 1.0f,
		1.0f, 0.0f,
	};
	float expected_data[] = {
		2.5f, 2.5f,
		2.5f, 0.0f,
	};
	float out_data[2 * 2];

	const float half[] = { 0.5f, 0.5f, 0.5f, 0.5f };
	const float two[] = { 2.0f, 2.0f, 2.0f, 0.5f };

	MultiplyEffect *mul_half = new MultiplyEffect();
	ASSERT_TRUE(mul_half->set_vec4("factor", half));
	
	MultiplyEffect *mul_two = new MultiplyEffect();
	ASSERT_TRUE(mul_two->set_vec4("factor", two));
	
	BouncingIdentityEffect *bounce = new BouncingIdentityEffect();

	EffectChainTester tester(nullptr, 2, 2);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	FlatInput *input = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, 2, 2);
	input->set_pixel_data(data);

	tester.get_chain()->add_input(input);
	tester.get_chain()->add_effect(mul_half, input);
	tester.get_chain()->add_effect(mul_two, input);
	tester.get_chain()->add_effect(bounce, mul_two);
	tester.get_chain()->add_effect(new AddEffect(), mul_half, bounce);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 2, 2);
}

// Constructs the graph
//
//                        FlatInput                               |
//                       /         \                              |
//  Downscale2xEffect (mipmaps)  Downscale2xEffect (no mipmaps)   |
//                      |           |                             |
//  Downscale2xEffect (mipmaps)  Downscale2xEffect (no mipmaps)   |
//                       \         /                              |
//                        AddEffect                               |
//
// and verifies that it gives the correct output. Due to the conflicting
// mipmap demands, EffectChain needs to make two phases; exactly where it's
// split is less important, though (this is a fairly obscure situation that
// is unlikely to happen in practice).
TEST(EffectChainTest, DiamondGraphWithConflictingMipmaps) {
	float data[] = {
		0.0f, 0.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 1.0f, 0.0f,
	};

	// Same situation as MipmapChainGetsSplit. The output of the two
	// downscales with no mipmaps looks like this:
	//
	//    0 0 0 0
	//    0 0 0 0
	//    0 0 0 0
	//    1 0 0 0
	//
	// and the one with mipmaps is 0.25 everywhere. Due to postmultiplied
	// alpha, we get the average even though we are using AddEffect.
	float expected_data[] = {
		0.125f, 0.125f, 0.125f, 0.125f,
		0.125f, 0.125f, 0.125f, 0.125f,
		0.125f, 0.125f, 0.125f, 0.125f,
		0.625f, 0.125f, 0.125f, 0.125f,
	};
	float out_data[4 * 4];

	float offset[] = { -0.5f / 4.0f, -0.5f / 4.0f };
	Downscale2xEffect *nomipmap1 = new Downscale2xEffect(Effect::CANNOT_ACCEPT_MIPMAPS);
	Downscale2xEffect *nomipmap2 = new Downscale2xEffect(Effect::CANNOT_ACCEPT_MIPMAPS);
	ASSERT_TRUE(nomipmap1->set_vec2("offset", offset));
	ASSERT_TRUE(nomipmap2->set_vec2("offset", offset));

	Downscale2xEffect *mipmap1 = new Downscale2xEffect(Effect::NEEDS_MIPMAPS);
	Downscale2xEffect *mipmap2 = new Downscale2xEffect(Effect::NEEDS_MIPMAPS);

	EffectChainTester tester(nullptr, 4, 4);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	FlatInput *input = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, 4, 4);
	input->set_pixel_data(data);

	tester.get_chain()->add_input(input);

	tester.get_chain()->add_effect(nomipmap1, input);
	tester.get_chain()->add_effect(nomipmap2, nomipmap1);

	tester.get_chain()->add_effect(mipmap1, input);
	tester.get_chain()->add_effect(mipmap2, mipmap1);

	tester.get_chain()->add_effect(new AddEffect(), nomipmap2, mipmap2);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 4, 4);
}

TEST(EffectChainTest, EffectUsedTwiceOnlyGetsOneGammaConversion) {
	float data[] = {
		0.735f, 0.0f,
		0.735f, 0.0f,
	};
	float expected_data[] = {
		0.0f, 0.5f,  // 0.5 and not 1.0, since AddEffect doesn't clamp alpha properly.
		0.0f, 0.5f,
	};
	float out_data[2 * 2];
	
	EffectChainTester tester(nullptr, 2, 2);
	tester.add_input(data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_sRGB);

	// MirrorEffect does not get linear light, so the conversions will be
	// inserted after it, not before.
	RewritingEffect<MirrorEffect> *effect = new RewritingEffect<MirrorEffect>();
	tester.get_chain()->add_effect(effect);

	Effect *identity1 = tester.get_chain()->add_effect(new IdentityEffect(), effect);
	Effect *identity2 = tester.get_chain()->add_effect(new IdentityEffect(), effect);
	tester.get_chain()->add_effect(new AddEffect(), identity1, identity2);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 2, 2);

	Node *node = effect->replaced_node;
	ASSERT_EQ(1u, node->incoming_links.size());
	ASSERT_EQ(1u, node->outgoing_links.size());
	EXPECT_EQ("FlatInput", node->incoming_links[0]->effect->effect_type_id());
	EXPECT_EQ("GammaExpansionEffect", node->outgoing_links[0]->effect->effect_type_id());
}

TEST(EffectChainTest, EffectUsedTwiceOnlyGetsOneColorspaceConversion) {
	float data[] = {
		0.5f, 0.0f,
		0.5f, 0.0f,
	};
	float expected_data[] = {
		0.0f, 0.5f,  // 0.5 and not 1.0, since AddEffect doesn't clamp alpha properly.
		0.0f, 0.5f,
	};
	float out_data[2 * 2];
	
	EffectChainTester tester(nullptr, 2, 2);
	tester.add_input(data, FORMAT_GRAYSCALE, COLORSPACE_REC_601_625, GAMMA_LINEAR);

	// MirrorEffect does not get linear light, so the conversions will be
	// inserted after it, not before.
	RewritingEffect<MirrorEffect> *effect = new RewritingEffect<MirrorEffect>();
	tester.get_chain()->add_effect(effect);

	Effect *identity1 = tester.get_chain()->add_effect(new IdentityEffect(), effect);
	Effect *identity2 = tester.get_chain()->add_effect(new IdentityEffect(), effect);
	tester.get_chain()->add_effect(new AddEffect(), identity1, identity2);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 2, 2);

	Node *node = effect->replaced_node;
	ASSERT_EQ(1u, node->incoming_links.size());
	ASSERT_EQ(1u, node->outgoing_links.size());
	EXPECT_EQ("FlatInput", node->incoming_links[0]->effect->effect_type_id());
	EXPECT_EQ("ColorspaceConversionEffect", node->outgoing_links[0]->effect->effect_type_id());
}

// An effect that does nothing, but requests texture bounce and stores
// its input size.
class SizeStoringEffect : public BouncingIdentityEffect {
public:
	SizeStoringEffect() : input_width(-1), input_height(-1) {}
	void inform_input_size(unsigned input_num, unsigned width, unsigned height) override {
		assert(input_num == 0);
		input_width = width;
		input_height = height;
	}
	string effect_type_id() const override { return "SizeStoringEffect"; }

	int input_width, input_height;
};

TEST(EffectChainTest, SameInputsGiveSameOutputs) {
	float data[2 * 2] = {
		0.0f, 0.0f,
		0.0f, 0.0f,
	};
	float out_data[4 * 3];
	
	EffectChainTester tester(nullptr, 4, 3);  // Note non-square aspect.

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	FlatInput *input1 = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, 2, 2);
	input1->set_pixel_data(data);
	
	FlatInput *input2 = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, 2, 2);
	input2->set_pixel_data(data);

	SizeStoringEffect *input_store = new SizeStoringEffect();

	tester.get_chain()->add_input(input1);
	tester.get_chain()->add_input(input2);
	tester.get_chain()->add_effect(new AddEffect(), input1, input2);
	tester.get_chain()->add_effect(input_store);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	EXPECT_EQ(2, input_store->input_width);
	EXPECT_EQ(2, input_store->input_height);
}

TEST(EffectChainTest, AspectRatioConversion) {
	float data1[4 * 3] = {
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
	};
	float data2[7 * 7] = {
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	};

	// The right conversion here is that the 7x7 image decides the size,
	// since it is the biggest, so everything is scaled up to 9x7
	// (keep the height, round the width 9.333 to 9). 
	float out_data[9 * 7];
	
	EffectChainTester tester(nullptr, 4, 3);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	FlatInput *input1 = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, 4, 3);
	input1->set_pixel_data(data1);
	
	FlatInput *input2 = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, 7, 7);
	input2->set_pixel_data(data2);

	SizeStoringEffect *input_store = new SizeStoringEffect();

	tester.get_chain()->add_input(input1);
	tester.get_chain()->add_input(input2);
	tester.get_chain()->add_effect(new AddEffect(), input1, input2);
	tester.get_chain()->add_effect(input_store);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	EXPECT_EQ(9, input_store->input_width);
	EXPECT_EQ(7, input_store->input_height);
}

// Tests that putting a BlueInput (constant color) into its own pass,
// which creates a phase that doesn't need texture coordinates,
// doesn't mess up a second phase that actually does.
TEST(EffectChainTest, FirstPhaseWithNoTextureCoordinates) {
	const int size = 2;
	float data[] = {
		1.0f,
		0.0f,
	};
	float expected_data[] = {
		1.0f, 1.0f, 2.0f, 2.0f,
		0.0f, 0.0f, 1.0f, 2.0f,
	};
	float out_data[size * 4];
	// First say that we have sRGB, linear input.
	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;
	FlatInput *input = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, 1, size);

	input->set_pixel_data(data);
	EffectChainTester tester(nullptr, 1, size);
	tester.get_chain()->add_input(new BlueInput());
	Effect *phase1_end = tester.get_chain()->add_effect(new BouncingIdentityEffect());
	tester.get_chain()->add_input(input);
	tester.get_chain()->add_effect(new AddEffect(), phase1_end, input);

	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);

	expect_equal(expected_data, out_data, 4, size);
}

// An effect that does nothing except changing its output sizes.
class VirtualResizeEffect : public Effect {
public:
	VirtualResizeEffect(int width, int height, int virtual_width, int virtual_height)
		: width(width),
		  height(height),
		  virtual_width(virtual_width),
		  virtual_height(virtual_height) {}
	string effect_type_id() const override { return "VirtualResizeEffect"; }
	string output_fragment_shader() override { return read_file("identity.frag"); }

	bool changes_output_size() const override { return true; }

	void get_output_size(unsigned *width, unsigned *height,
	                     unsigned *virtual_width, unsigned *virtual_height) const override {
		*width = this->width;
		*height = this->height;
		*virtual_width = this->virtual_width;
		*virtual_height = this->virtual_height;
	}

private:
	int width, height, virtual_width, virtual_height;
};

TEST(EffectChainTest, VirtualSizeIsSentOnToInputs) {
	const int size = 2, bigger_size = 3;
	float data[size * size] = {
		1.0f, 0.0f,
		0.0f, 1.0f,
	};
	float out_data[size * size];
	
	EffectChainTester tester(data, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);

	SizeStoringEffect *size_store = new SizeStoringEffect();

	tester.get_chain()->add_effect(new VirtualResizeEffect(size, size, bigger_size, bigger_size));
	tester.get_chain()->add_effect(size_store);
	tester.get_chain()->add_effect(new VirtualResizeEffect(size, size, size, size));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	EXPECT_EQ(bigger_size, size_store->input_width);
	EXPECT_EQ(bigger_size, size_store->input_height);

	// If the resize is implemented as non-virtual, we'll fail here,
	// since bilinear scaling from 2x2 → 3x3 → 2x2 is not very exact.
	expect_equal(data, out_data, size, size);
}

// An effect that is like VirtualResizeEffect, but always has virtual and real
// sizes the same (and promises this).
class NonVirtualResizeEffect : public VirtualResizeEffect {
public:
	NonVirtualResizeEffect(int width, int height)
		: VirtualResizeEffect(width, height, width, height) {}
	string effect_type_id() const override { return "NonVirtualResizeEffect"; }
	bool sets_virtual_output_size() const override { return false; }
};

// An effect that promises one-to-one sampling (unlike IdentityEffect).
class OneToOneEffect : public Effect {
public:
	OneToOneEffect() {}
	string effect_type_id() const override { return "OneToOneEffect"; }
	string output_fragment_shader() override { return read_file("identity.frag"); }
	bool strong_one_to_one_sampling() const override { return true; }
};

TEST_P(WithAndWithoutComputeShaderTest, NoBounceWithOneToOneSampling) {
	const int size = 2;
	float data[size * size] = {
		1.0f, 0.0f,
		0.0f, 1.0f,
	};
	float out_data[size * size];

	EffectChainTester tester(data, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);

	RewritingEffect<OneToOneEffect> *effect1 = new RewritingEffect<OneToOneEffect>();
	RewritingEffect<OneToOneEffect> *effect2 = new RewritingEffect<OneToOneEffect>();

	if (GetParam() == "compute") {
		if (!movit_compute_shaders_supported) {
			fprintf(stderr, "Skipping test; no support for compile shaders.\n");
			return;
		}
		tester.get_chain()->add_effect(new IdentityComputeEffect());
	} else {
		tester.get_chain()->add_effect(new NonVirtualResizeEffect(size, size));
	}
	tester.get_chain()->add_effect(effect1);
	tester.get_chain()->add_effect(effect2);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(data, out_data, size, size);

	// The first OneToOneEffect should be in the same phase as its input.
	ASSERT_EQ(1u, effect1->replaced_node->incoming_links.size());
	EXPECT_EQ(effect1->replaced_node->incoming_links[0]->containing_phase,
	          effect1->replaced_node->containing_phase);

	// The second OneToOneEffect, too.
	EXPECT_EQ(effect1->replaced_node->containing_phase,
	          effect2->replaced_node->containing_phase);
}

TEST(EffectChainTest, BounceWhenOneToOneIsBroken) {
	const int size = 2;
	float data[size * size] = {
		1.0f, 0.0f,
		0.0f, 1.0f,
	};
	float out_data[size * size];

	EffectChainTester tester(data, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);

	RewritingEffect<OneToOneEffect> *effect1 = new RewritingEffect<OneToOneEffect>();
	RewritingEffect<OneToOneEffect> *effect2 = new RewritingEffect<OneToOneEffect>();
	RewritingEffect<IdentityEffect> *effect3 = new RewritingEffect<IdentityEffect>();
	RewritingEffect<OneToOneEffect> *effect4 = new RewritingEffect<OneToOneEffect>();

	tester.get_chain()->add_effect(new NonVirtualResizeEffect(size, size));
	tester.get_chain()->add_effect(effect1);
	tester.get_chain()->add_effect(effect2);
	tester.get_chain()->add_effect(effect3);
	tester.get_chain()->add_effect(effect4);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(data, out_data, size, size);

	// The NonVirtualResizeEffect should be in a different phase from
	// the IdentityEffect (since the latter is not one-to-one),
	// ie., the chain should be broken somewhere between them, but exactly
	// where doesn't matter.
	ASSERT_EQ(1u, effect1->replaced_node->incoming_links.size());
	EXPECT_NE(effect1->replaced_node->incoming_links[0]->containing_phase,
	          effect3->replaced_node->containing_phase);

	// The last OneToOneEffect should also be in the same phase as the
	// IdentityEffect (the phase was already broken).
	EXPECT_EQ(effect3->replaced_node->containing_phase,
	          effect4->replaced_node->containing_phase);
}

// Does not use EffectChainTest, so that it can construct an EffectChain without
// a shared ResourcePool (which is also properly destroyed afterwards).
// Also turns on debugging to test that code path.
TEST(EffectChainTest, IdentityWithOwnPool) {
	const int width = 3, height = 2;
	float data[] = {
		0.0f, 0.25f, 0.3f,
		0.75f, 1.0f, 1.0f,
	};
	const float expected_data[] = {
		0.75f, 1.0f, 1.0f,
		0.0f, 0.25f, 0.3f,
	};
	float out_data[6], temp[6 * 4];

	EffectChain chain(width, height);
	MovitDebugLevel old_movit_debug_level = movit_debug_level;
	movit_debug_level = MOVIT_DEBUG_ON;

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	FlatInput *input = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, width, height);
	input->set_pixel_data(data);
	chain.add_input(input);
	chain.add_output(format, OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);

	GLuint texnum, fbo;
	glGenTextures(1, &texnum);
	check_error();
	glBindTexture(GL_TEXTURE_2D, texnum);
	check_error();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
	check_error();

	glGenFramebuffers(1, &fbo);
	check_error();
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	check_error();
	glFramebufferTexture2D(
		GL_FRAMEBUFFER,
		GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D,
		texnum,
		0);
	check_error();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	check_error();

	chain.finalize();

	chain.render_to_fbo(fbo, width, height);

	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	check_error();
	glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, temp);
	check_error();
	for (unsigned i = 0; i < 6; ++i) {
		out_data[i] = temp[i * 4];
	}

	expect_equal(expected_data, out_data, width, height);

	// Reset the debug status again.
	movit_debug_level = old_movit_debug_level;
}

// A dummy effect whose only purpose is to test sprintf decimal behavior.
class PrintfingBlueEffect : public Effect {
public:
	PrintfingBlueEffect() {}
	string effect_type_id() const override { return "PrintfingBlueEffect"; }
	string output_fragment_shader() override {
		stringstream ss;
		ss.imbue(locale("C"));
		ss.precision(8);
		ss << "vec4 FUNCNAME(vec2 tc) { return vec4("
		   << 0.0f << ", " << 0.0f << ", "
		   << 0.5f << ", " << 1.0f << "); }\n";
		return ss.str();
	}
};

TEST(EffectChainTest, StringStreamLocalesWork) {
	// An example of a locale with comma instead of period as decimal separator.
	// Obviously, if you run on a machine without this locale available,
	// the test will always succeed. Note that the OpenGL driver might call
	// setlocale() behind-the-scenes, and that might corrupt the returned
	// pointer, so we need to take our own copy of it here.
	char *saved_locale = setlocale(LC_ALL, "nb_NO.UTF_8");
	if (saved_locale == nullptr) {
		// The locale wasn't available.
		return;
	}
	saved_locale = strdup(saved_locale);
	float data[] = {
		0.0f, 0.0f, 0.0f, 0.0f,
	};
	float expected_data[] = {
		0.0f, 0.0f, 0.5f, 1.0f,
	};
	float out_data[4];
	EffectChainTester tester(data, 1, 1, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.get_chain()->add_effect(new PrintfingBlueEffect());
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 4, 1);

	setlocale(LC_ALL, saved_locale);
	free(saved_locale);
}

TEST(EffectChainTest, sRGBIntermediate) {
	float data[] = {
		0.0f, 0.5f, 0.0f, 1.0f,
	};
	float out_data[4];
	EffectChainTester tester(data, 1, 1, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.get_chain()->set_intermediate_format(GL_SRGB8);
	tester.get_chain()->add_effect(new IdentityEffect());
	tester.get_chain()->add_effect(new BouncingIdentityEffect());
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	EXPECT_GE(fabs(out_data[1] - data[1]), 1e-3)
	    << "Expected sRGB not to be able to represent 0.5 exactly (got " << out_data[1] << ")";
	EXPECT_LT(fabs(out_data[1] - data[1]), 0.1f)
	    << "Expected sRGB to be able to represent 0.5 approximately (got " << out_data[1] << ")";

	// This state should have been preserved.
	EXPECT_FALSE(glIsEnabled(GL_FRAMEBUFFER_SRGB));
}

// An effect that is like IdentityEffect, but also does not require linear light.
class PassThroughEffect : public IdentityEffect {
public:
	PassThroughEffect() {}
	string effect_type_id() const override { return "PassThroughEffect"; }
	bool needs_linear_light() const override { return false; }
	AlphaHandling alpha_handling() const override { return DONT_CARE_ALPHA_TYPE; }
};

// Same, just also bouncing.
class BouncingPassThroughEffect : public BouncingIdentityEffect {
public:
	BouncingPassThroughEffect() {}
	string effect_type_id() const override { return "BouncingPassThroughEffect"; }
	bool needs_linear_light() const override { return false; }
	bool needs_texture_bounce() const override { return true; }
	AlphaHandling alpha_handling() const override { return DONT_CARE_ALPHA_TYPE; }
};

TEST(EffectChainTest, Linear10bitIntermediateAccuracy) {
	// Note that we do the comparison in sRGB space, which is what we
	// typically would want; however, we do the sRGB conversion ourself
	// to avoid compounding errors from shader conversions into the
	// analysis.
	const int size = 4096;  // 12-bit.
	float linear_data[size], data[size], out_data[size];

	for (int i = 0; i < size; ++i) {
		linear_data[i] = i / double(size - 1);
		data[i] = srgb_to_linear(linear_data[i]);
	}

	EffectChainTester tester(data, size, 1, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, GL_RGBA32F);
	tester.get_chain()->set_intermediate_format(GL_RGB10_A2);
	tester.get_chain()->add_effect(new IdentityEffect());
	tester.get_chain()->add_effect(new BouncingIdentityEffect());
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	for (int i = 0; i < size; ++i) {
		out_data[i] = linear_to_srgb(out_data[i]);
	}

	// This maximum error is pretty bad; about 6.5 levels of a 10-bit sRGB
	// framebuffer. (Slightly more on NVIDIA cards.)
	expect_equal(linear_data, out_data, size, 1, 7.5e-3, 2e-5);
}

TEST_P(WithAndWithoutComputeShaderTest, SquareRoot10bitIntermediateAccuracy) {
	// Note that we do the comparison in sRGB space, which is what we
	// typically would want; however, we do the sRGB conversion ourself
	// to avoid compounding errors from shader conversions into the
	// analysis.
	const int size = 4096;  // 12-bit.
	float linear_data[size], data[size], out_data[size];

	for (int i = 0; i < size; ++i) {
		linear_data[i] = i / double(size - 1);
		data[i] = srgb_to_linear(linear_data[i]);
	}

	EffectChainTester tester(data, size, 1, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, GL_RGBA32F);
	tester.get_chain()->set_intermediate_format(GL_RGB10_A2, SQUARE_ROOT_FRAMEBUFFER_TRANSFORMATION);
	if (GetParam() == "compute") {
		if (!movit_compute_shaders_supported) {
			fprintf(stderr, "Skipping test; no support for compile shaders.\n");
			return;
		}
		tester.get_chain()->add_effect(new IdentityComputeEffect());
	} else {
		tester.get_chain()->add_effect(new IdentityEffect());
	}
	tester.get_chain()->add_effect(new BouncingIdentityEffect());
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	for (int i = 0; i < size; ++i) {
		out_data[i] = linear_to_srgb(out_data[i]);
	}

	// This maximum error is much better; about 0.7 levels of a 10-bit sRGB
	// framebuffer (ideal would be 0.5). That is an order of magnitude better
	// than in the linear test above. The RMS error is much better, too.
	expect_equal(linear_data, out_data, size, 1, 7.5e-4, 5e-6);
}

TEST(EffectChainTest, SquareRootIntermediateIsTurnedOffForNonLinearData) {
	const int size = 256;  // 8-bit.
	float data[size], out_data[size];

	for (int i = 0; i < size; ++i) {
		data[i] = i / double(size - 1);
	}

	EffectChainTester tester(data, size, 1, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_REC_601, GL_RGBA32F);
	tester.get_chain()->set_intermediate_format(GL_RGB8, SQUARE_ROOT_FRAMEBUFFER_TRANSFORMATION);
	tester.get_chain()->add_effect(new PassThroughEffect());
	tester.get_chain()->add_effect(new BouncingPassThroughEffect());
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_REC_601);

	// The data should be passed through nearly exactly, since there is no effect
	// on the path that requires linear light. (Actually, it _is_ exact modulo
	// fp32 errors, but the error bounds is strictly _less than_, not zero.)
	expect_equal(data, out_data, size, 1, 1e-6, 1e-6);
}

// An effect that stores which program number was last run under.
class RecordingIdentityEffect : public Effect {
public:
	RecordingIdentityEffect() {}
	string effect_type_id() const override { return "RecordingIdentityEffect"; }
	string output_fragment_shader() override { return read_file("identity.frag"); }

	GLuint last_glsl_program_num;
	void set_gl_state(GLuint glsl_program_num, const std::string& prefix, unsigned *sampler_num) override
	{
		last_glsl_program_num = glsl_program_num;
	}
};

TEST(EffectChainTest, ProgramsAreClonedForMultipleThreads) {
	float data[] = {
		0.0f, 0.25f, 0.3f,
		0.75f, 1.0f, 1.0f,
	};
	float out_data[6];
	EffectChainTester tester(data, 3, 2, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	RecordingIdentityEffect *effect = new RecordingIdentityEffect();
	tester.get_chain()->add_effect(effect);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(data, out_data, 3, 2);

	ASSERT_NE(0u, effect->last_glsl_program_num);

	// Now pretend some other effect is using this program number;
	// ResourcePool will then need to clone it.
	ResourcePool *resource_pool = tester.get_chain()->get_resource_pool();
	GLuint master_program_num = resource_pool->use_glsl_program(effect->last_glsl_program_num);
	EXPECT_EQ(effect->last_glsl_program_num, master_program_num);

	// Re-run should still give the correct data, but it should have run
	// with a different program.
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);
	expect_equal(data, out_data, 3, 2);
	EXPECT_NE(effect->last_glsl_program_num, master_program_num);

	// Release the program, and check one final time.
	resource_pool->unuse_glsl_program(master_program_num);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);
	expect_equal(data, out_data, 3, 2);
}

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

TEST(ComputeShaderTest, Render8BitTo8Bit) {
	uint8_t data[] = {
		14, 200, 80,
		90, 100, 110,
	};
	uint8_t out_data[6];
	EffectChainTester tester(nullptr, 3, 2, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, GL_RGBA8);
	if (!movit_compute_shaders_supported) {
		fprintf(stderr, "Skipping test; no support for compile shaders.\n");
		return;
	}
	tester.add_input(data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, 3, 2);
	tester.get_chain()->add_effect(new IdentityAlphaComputeEffect());
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(data, out_data, 3, 2);
}

// A compute shader to mirror the inputs, in 2x2 blocks.
class MirrorComputeEffect : public Effect {
public:
	MirrorComputeEffect() {}
	string effect_type_id() const override { return "MirrorComputeEffect"; }
	bool is_compute_shader() const override { return true; }
	string output_fragment_shader() override { return read_file("mirror.comp"); }
	void get_compute_dimensions(unsigned output_width, unsigned output_height,
	                            unsigned *x, unsigned *y, unsigned *z) const override {
		*x = output_width / 2;
		*y = output_height / 2;
		*z = 1;
	}
};

TEST(ComputeShaderTest, ComputeThenOneToOne) {
	float data[] = {
		0.0f, 0.25f, 0.3f, 0.8f,
		0.75f, 1.0f, 1.0f, 0.2f,
	};
	float expected_data[] = {
		0.8f, 0.3f, 0.25f, 0.0f,
		0.2f, 1.0f, 1.0f, 0.75f,
	};
	float out_data[8];
	EffectChainTester tester(data, 4, 2, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	if (!movit_compute_shaders_supported) {
		fprintf(stderr, "Skipping test; no support for compile shaders.\n");
		return;
	}
	tester.get_chain()->add_effect(new MirrorComputeEffect());
	tester.get_chain()->add_effect(new OneToOneEffect());
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 4, 2);
}

// A compute shader that also resizes its input, taking the upper-left pixel
// of every 2x2 group. (The shader is hard-coded to 4x2 input for simplicity.)
class Downscale2xComputeEffect : public Effect {
public:
	Downscale2xComputeEffect() {}
	string effect_type_id() const override { return "Downscale2xComputeEffect"; }
	bool is_compute_shader() const override { return true; }
	string output_fragment_shader() override { return read_file("downscale2x.comp"); }
	bool changes_output_size() const override { return true; }
	void inform_input_size(unsigned input_num, unsigned width, unsigned height) override
	{
		this->width = width;
		this->height = height;
	}
	void get_output_size(unsigned *width, unsigned *height,
	                     unsigned *virtual_width, unsigned *virtual_height) const override {
                *width = *virtual_width = this->width / 2;
                *height = *virtual_height = this->height / 2;
        }

private:
	unsigned width, height;
};

// Even if the compute shader is not the last effect, it's the one that should decide
// the output size of the phase.
TEST(ComputeShaderTest, ResizingComputeThenOneToOne) {
	float data[] = {
		0.0f, 0.25f, 0.3f, 0.8f,
		0.75f, 1.0f, 1.0f, 0.2f,
	};
	float expected_data[] = {
		0.0f, 0.3f,
	};
	float out_data[2];
	EffectChainTester tester(nullptr, 2, 1);
	if (!movit_compute_shaders_supported) {
		fprintf(stderr, "Skipping test; no support for compile shaders.\n");
		return;
	}
	tester.add_input(data, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, 4, 2);

	RewritingEffect<Downscale2xComputeEffect> *downscale_effect = new RewritingEffect<Downscale2xComputeEffect>();
	tester.get_chain()->add_effect(downscale_effect);
	tester.get_chain()->add_effect(new OneToOneEffect());
	tester.get_chain()->add_effect(new BouncingIdentityEffect());
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 2, 1);

	Phase *phase = downscale_effect->replaced_node->containing_phase;
	EXPECT_EQ(2u, phase->output_width);
	EXPECT_EQ(1u, phase->output_height);
}

class StrongOneToOneAddEffect : public AddEffect {
public:
	StrongOneToOneAddEffect() {}
	string effect_type_id() const override { return "StrongOneToOneAddEffect"; }
	bool strong_one_to_one_sampling() const override { return true; }
};

TEST(ComputeShaderTest, NoTwoComputeInSamePhase) {
	float data[] = {
		0.0f, 0.25f, 0.3f, 0.8f,
		0.75f, 1.0f, 1.0f, 0.2f,
	};
	float expected_data[] = {
		0.0f, 0.3f,
	};
	float out_data[2];

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	EffectChainTester tester(nullptr, 2, 1);
	if (!movit_compute_shaders_supported) {
		fprintf(stderr, "Skipping test; no support for compile shaders.\n");
		return;
	}

	FlatInput *input1 = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, 4, 2);
	input1->set_pixel_data(data);
	tester.get_chain()->add_input(input1);
	Effect *downscale1 = tester.get_chain()->add_effect(new Downscale2xComputeEffect());

	FlatInput *input2 = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, 4, 2);
	input2->set_pixel_data(data);
	tester.get_chain()->add_input(input2);
	Effect *downscale2 = tester.get_chain()->add_effect(new Downscale2xComputeEffect());

	tester.get_chain()->add_effect(new StrongOneToOneAddEffect(), downscale1, downscale2);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);
	expect_equal(expected_data, out_data, 2, 1);
}

// Like the previous test, but the adder effect is not directly connected
// to the compute shaders (so the status has to be propagated through those effects).
TEST(ComputeShaderTest, NoTwoComputeInSamePhaseIndirect) {
	float data[] = {
		0.0f, 0.25f, 0.3f, 0.8f,
		0.75f, 1.0f, 1.0f, 0.2f,
	};
	float expected_data[] = {
		0.0f, 0.3f,
	};
	float out_data[2];

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	EffectChainTester tester(nullptr, 2, 1);
	if (!movit_compute_shaders_supported) {
		fprintf(stderr, "Skipping test; no support for compile shaders.\n");
		return;
	}

	FlatInput *input1 = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, 4, 2);
	input1->set_pixel_data(data);
	tester.get_chain()->add_input(input1);
	tester.get_chain()->add_effect(new Downscale2xComputeEffect());
	Effect *identity1 = tester.get_chain()->add_effect(new OneToOneEffect());

	FlatInput *input2 = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, 4, 2);
	input2->set_pixel_data(data);
	tester.get_chain()->add_input(input2);
	tester.get_chain()->add_effect(new Downscale2xComputeEffect());
	Effect *identity2 = tester.get_chain()->add_effect(new OneToOneEffect());

	tester.get_chain()->add_effect(new StrongOneToOneAddEffect(), identity1, identity2);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);
	expect_equal(expected_data, out_data, 2, 1);
}

// Like the previous test, but the adder is not strong one-to-one
// (so there are two different compute shader inputs, but none of them
// are in the same phase).
TEST(ComputeShaderTest, BounceTextureFromTwoComputeShaders) {
	float data[] = {
		0.0f, 0.25f, 0.3f, 0.8f,
		0.75f, 1.0f, 1.0f, 0.2f,
	};
	float expected_data[] = {
		0.0f, 0.3f,
	};
	float out_data[2];

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	EffectChainTester tester(nullptr, 2, 1);
	if (!movit_compute_shaders_supported) {
		fprintf(stderr, "Skipping test; no support for compile shaders.\n");
		return;
	}

	FlatInput *input1 = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, 4, 2);
	input1->set_pixel_data(data);
	tester.get_chain()->add_input(input1);
	tester.get_chain()->add_effect(new Downscale2xComputeEffect());
	Effect *identity1 = tester.get_chain()->add_effect(new OneToOneEffect());

	FlatInput *input2 = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, 4, 2);
	input2->set_pixel_data(data);
	tester.get_chain()->add_input(input2);
	tester.get_chain()->add_effect(new Downscale2xComputeEffect());
	Effect *identity2 = tester.get_chain()->add_effect(new OneToOneEffect());

	tester.get_chain()->add_effect(new AddEffect(), identity1, identity2);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);
	expect_equal(expected_data, out_data, 2, 1);
}

// Requires mipmaps, but is otherwise like IdentityEffect.
class MipmapNeedingIdentityEffect : public IdentityEffect {
public:
	MipmapNeedingIdentityEffect() {}
	MipmapRequirements needs_mipmaps() const override { return NEEDS_MIPMAPS; }
	string effect_type_id() const override { return "MipmapNeedingIdentityEffect"; }
	bool strong_one_to_one_sampling() const override { return true; }
};

TEST(ComputeShaderTest, StrongOneToOneButStillNotChained) {
	float data[] = {
		0.0f, 0.25f, 0.3f, 0.8f,
		0.75f, 1.0f, 1.0f, 0.2f,
	};
	float out_data[8];

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	EffectChainTester tester(nullptr, 4, 2);
	if (!movit_compute_shaders_supported) {
		fprintf(stderr, "Skipping test; no support for compile shaders.\n");
		return;
	}

	FlatInput *input1 = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, 4, 2);
	input1->set_pixel_data(data);
	tester.get_chain()->add_input(input1);
	Effect *compute_effect = tester.get_chain()->add_effect(new IdentityComputeEffect());

	FlatInput *input2 = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, 4, 2);
	input2->set_pixel_data(data);
	tester.get_chain()->add_input(input2);

	// Not chained with the compute shader because MipmapNeedingIdentityEffect comes in
	// the same phase, and compute shaders cannot supply mipmaps.
	tester.get_chain()->add_effect(new StrongOneToOneAddEffect(), compute_effect, input2);
	tester.get_chain()->add_effect(new MipmapNeedingIdentityEffect());

	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);
	expect_equal(data, out_data, 4, 2);
}

TEST(EffectChainTest, BounceResetsMipmapNeeds) {
	float data[] = {
		0.0f, 0.25f,
		0.75f, 1.0f,
	};
	float out_data[1];

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	NonMipmapCapableInput *input = new NonMipmapCapableInput(format, FORMAT_GRAYSCALE, GL_FLOAT, 2, 2);
	input->set_pixel_data(data);

	RewritingEffect<IdentityEffect> *identity = new RewritingEffect<IdentityEffect>();

	RewritingEffect<ResizeEffect> *downscale = new RewritingEffect<ResizeEffect>();  // Needs mipmaps.
	ASSERT_TRUE(downscale->effect->set_int("width", 1));
	ASSERT_TRUE(downscale->effect->set_int("height", 1));

	EffectChainTester tester(nullptr, 1, 1);
	tester.get_chain()->add_input(input);
	tester.get_chain()->add_effect(identity);
	tester.get_chain()->add_effect(downscale);
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	Node *input_node = identity->replaced_node->incoming_links[0];

	// The ResizeEffect needs mipmaps. Normally, that would mean that it should
	// propagate this tatus down through the IdentityEffect. However, since we
	// bounce (due to the resize), the dependency breaks there, and we don't
	// need to bounce again between the input and the IdentityEffect.
	EXPECT_EQ(input_node->containing_phase,
	          identity->replaced_node->containing_phase);
	EXPECT_NE(identity->replaced_node->containing_phase,
	          downscale->replaced_node->containing_phase);
}

}  // namespace movit
