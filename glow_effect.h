#ifndef _GLOW_EFFECT_H
#define _GLOW_EFFECT_H 1

// Glow: Simply add a blurred version of the image to itself.

#include "effect.h"

class BlurEffect;
class MixEffect;

class GlowEffect : public Effect {
public:
	GlowEffect();
	virtual std::string effect_type_id() const { return "GlowEffect"; }

	virtual bool needs_srgb_primaries() const { return false; }

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
	MixEffect *mix;
};

#endif // !defined(_GLOW_EFFECT_H)
