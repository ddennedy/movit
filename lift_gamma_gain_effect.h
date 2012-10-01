#ifndef _LIFT_GAMMA_GAIN_EFFECT_H
#define _LIFT_GAMMA_GAIN_EFFECT_H 1

#include "effect.h"

class LiftGammaGainEffect : public Effect {
public:
	LiftGammaGainEffect();
	std::string output_glsl();

private:
	RGBTriplet lift, gamma, gain;
	float saturation;
};

#endif // !defined(_LIFT_GAMMA_GAIN_EFFECT_H)
