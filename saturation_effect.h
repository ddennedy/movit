#ifndef _SATURATION_EFFECT_H
#define _SATURATION_EFFECT_H 1

#include "effect.h"

class SaturationEffect : public Effect {
public:
	SaturationEffect();
	std::string output_glsl();

private:
	float saturation;
};

#endif // !defined(_SATURATION_EFFECT_H)
