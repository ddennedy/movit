#ifndef _DIFFUSION_EFFECT_H
#define _DIFFUSION_EFFECT_H 1

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

#include "effect.h"

class BlurEffect;
class OverlayMatteEffect;

class DiffusionEffect : public Effect {
public:
	DiffusionEffect();
	virtual std::string effect_type_id() const { return "DiffusionEffect"; }

	virtual void rewrite_graph(EffectChain *graph, Node *self);
	virtual bool set_float(const std::string &key, float value);
	
	virtual std::string output_fragment_shader() {
		assert(false);
	}
	virtual void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num) {
		assert(false);
	}

private:
	BlurEffect *blur;
	OverlayMatteEffect *overlay_matte;
};

// Used internally by DiffusionEffect; combines the blurred and the original
// version using the original as a matte.
class OverlayMatteEffect : public Effect {
public:
	OverlayMatteEffect();
	virtual std::string effect_type_id() const { return "OverlayMatteEffect"; }
	std::string output_fragment_shader();
	virtual AlphaHandling alpha_handling() const { return INPUT_PREMULTIPLIED_ALPHA_KEEP_BLANK; }

	unsigned num_inputs() const { return 2; }

private:
	float blurred_mix_amount;
};


#endif // !defined(_DIFFUSION_EFFECT_H)
