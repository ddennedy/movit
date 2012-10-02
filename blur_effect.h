#ifndef _BLUR_EFFECT_H
#define _BLUR_EFFECT_H 1

#include "effect.h"

class BlurEffect : public Effect {
public:
	BlurEffect();
	std::string output_fragment_shader();

	virtual bool needs_many_samples() const { return true; }
	virtual bool needs_mipmaps() const { return true; }

	void set_uniforms(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num);

private:
	float radius;
};

#endif // !defined(_BLUR_EFFECT_H)
