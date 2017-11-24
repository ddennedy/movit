#ifndef _MOVIT_DIFFUSION_EFFECT_H
#define _MOVIT_DIFFUSION_EFFECT_H 1

// There are many different effects that go under the name of "diffusion",
// seemingly all of the inspired by the effect you get when you put a
// diffusion filter in front of your camera lens. The effect most people
// want is a general flattening/smoothing of the light, and reduction of
// fine detail (most notably, blemishes in people's skin), without ruining
// edges, which a regular blur would do.
//
// We do a relatively simple version, sometimes known as "white diffusion",
// where we first blur the picture, and then overlay it on the original
// using the original as a matte.

#include <epoxy/gl.h>
#include <assert.h>
#include <string>

#include "effect.h"

namespace movit {

class BlurEffect;
class EffectChain;
class Node;
class OverlayMatteEffect;

class DiffusionEffect : public Effect {
public:
	DiffusionEffect();
	~DiffusionEffect();
	std::string effect_type_id() const override { return "DiffusionEffect"; }

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
	OverlayMatteEffect *overlay_matte;
	bool owns_overlay_matte;
};

// Used internally by DiffusionEffect; combines the blurred and the original
// version using the original as a matte.
class OverlayMatteEffect : public Effect {
public:
	OverlayMatteEffect();
	std::string effect_type_id() const override { return "OverlayMatteEffect"; }
	std::string output_fragment_shader() override;
	AlphaHandling alpha_handling() const override { return INPUT_PREMULTIPLIED_ALPHA_KEEP_BLANK; }

	unsigned num_inputs() const override { return 2; }
	bool strong_one_to_one_sampling() const override { return true; }

private:
	float blurred_mix_amount;
};

}  // namespace movit

#endif // !defined(_MOVIT_DIFFUSION_EFFECT_H)
