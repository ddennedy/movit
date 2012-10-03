#ifndef _BLUR_EFFECT_H
#define _BLUR_EFFECT_H 1

#include "effect.h"

class SingleBlurPassEffect;

class BlurEffect : public Effect {
public:
	BlurEffect();

	virtual std::string output_fragment_shader() {
		assert(false);
	}
	virtual void set_uniforms(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num) {
		assert(false);
	}

	virtual bool needs_many_samples() const { return true; }
	virtual bool needs_mipmaps() const { return true; }
	virtual void add_self_to_effect_chain(std::vector<Effect *> *chain);
	virtual bool set_float(const std::string &key, float value);
	
private:
	SingleBlurPassEffect *hpass, *vpass;
};

class SingleBlurPassEffect : public Effect {
public:
	SingleBlurPassEffect();
	std::string output_fragment_shader();

	virtual bool needs_many_samples() const { return true; }
	virtual bool needs_mipmaps() const { return true; }

	void set_uniforms(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num);
	
	enum Direction { HORIZONTAL = 0, VERTICAL = 1 };

private:
	float radius;
	Direction direction;
};

#endif // !defined(_BLUR_EFFECT_H)
