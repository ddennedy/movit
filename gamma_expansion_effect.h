#ifndef _GAMMA_EXPANSION_EFFECT_H 
#define _GAMMA_EXPANSION_EFFECT_H 1

#include "effect.h"
#include "effect_chain.h"

class GammaExpansionEffect : public Effect {
public:
	GammaExpansionEffect();
	std::string output_glsl();

	virtual bool needs_linear_light() { return false; }
	virtual bool needs_srgb_primaries() { return false; }

private:
	GammaCurve source_curve;
};

#endif // !defined(_GAMMA_EXPANSION_EFFECT_H)
