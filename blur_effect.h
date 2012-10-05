#ifndef _BLUR_EFFECT_H
#define _BLUR_EFFECT_H 1

// A separable 2D blur implemented by a combination of mipmap filtering
// and convolution (essentially giving a convolution with a piecewise linear
// approximation to the true impulse response).
//
// Works in two passes; first horizontal, then vertical (BlurEffect,
// which is what the user is intended to use, instantiates two copies of
// SingleBlurPassEffect behind the scenes).

#include "effect.h"

class SingleBlurPassEffect;

class BlurEffect : public Effect {
public:
	BlurEffect();

	virtual bool needs_srgb_primaries() const { return false; }

	virtual std::string output_fragment_shader() {
		assert(false);
	}
	virtual void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num) {
		assert(false);
	}

	virtual bool needs_texture_bounce() const { return true; }
	virtual bool needs_mipmaps() const { return true; }
	virtual void add_self_to_effect_chain(EffectChain *chain, const std::vector<Effect *> &input);
	virtual bool set_float(const std::string &key, float value);
	
private:
	SingleBlurPassEffect *hpass, *vpass;
};

class SingleBlurPassEffect : public Effect {
public:
	SingleBlurPassEffect();
	std::string output_fragment_shader();

	virtual bool needs_srgb_primaries() const { return false; }
	virtual bool needs_texture_bounce() const { return true; }
	virtual bool needs_mipmaps() const { return true; }

	void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num);
	void clear_gl_state();
	
	enum Direction { HORIZONTAL = 0, VERTICAL = 1 };

private:
	float radius;
	Direction direction;
};

#endif // !defined(_BLUR_EFFECT_H)
