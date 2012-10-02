#ifndef _LIFT_GAMMA_GAIN_EFFECT_H
#define _LIFT_GAMMA_GAIN_EFFECT_H 1

#include "effect.h"

class LiftGammaGainEffect : public Effect {
public:
	LiftGammaGainEffect();
	std::string output_fragment_shader();

	void set_uniforms(GLuint glsl_program_num, const std::string &prefix);

private:
	RGBTriplet lift, gamma, gain;
};

#endif // !defined(_LIFT_GAMMA_GAIN_EFFECT_H)
