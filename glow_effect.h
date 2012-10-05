#ifndef _GLOW_EFFECT_H
#define _GLOW_EFFECT_H 1

// Glow: Simply add a blurred version of the image to itself.

#include "effect.h"

class BlurEffect;
class MixEffect;

class GlowEffect : public Effect {
public:
	GlowEffect();

	virtual bool needs_srgb_primaries() const { return false; }

	virtual void add_self_to_effect_chain(EffectChain *chain, const std::vector<Effect *> &input);
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
