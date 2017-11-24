#ifndef _MOVIT_GLOW_EFFECT_H
#define _MOVIT_GLOW_EFFECT_H 1

// Glow: Cut out the highlights of the image (everything above a certain threshold),
// blur them, and overlay them onto the original image.

#include <epoxy/gl.h>
#include <assert.h>
#include <string>

#include "effect.h"

namespace movit {

class BlurEffect;
class EffectChain;
class HighlightCutoffEffect;
class MixEffect;
class Node;

class GlowEffect : public Effect {
public:
	GlowEffect();
	std::string effect_type_id() const override { return "GlowEffect"; }

	bool needs_srgb_primaries() const override { return false; }

	void rewrite_graph(EffectChain *graph, Node *self) override;
	bool set_float(const std::string &key, float value) override;

	std::string output_fragment_shader() override {
		assert(false);
	}
	void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num) override {
		assert(false);
	}

private:
	BlurEffect *blur;
	HighlightCutoffEffect *cutoff;
	MixEffect *mix;
};

// An effect that cuts out only the highlights of an image;
// anything at the cutoff or below is set to 0.0, and then all other pixels
// get the cutoff subtracted. Used only as part of GlowEffect.

class HighlightCutoffEffect : public Effect {
public:
	HighlightCutoffEffect();
	std::string effect_type_id() const override { return "HighlightCutoffEffect"; }
	std::string output_fragment_shader() override;
	
	AlphaHandling alpha_handling() const override { return INPUT_PREMULTIPLIED_ALPHA_KEEP_BLANK; }
	bool strong_one_to_one_sampling() const override { return true; }

private:
	float cutoff;
};

}  // namespace movit

#endif // !defined(_MOVIT_GLOW_EFFECT_H)
