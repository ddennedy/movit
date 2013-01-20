#ifndef _GAMMA_EXPANSION_EFFECT_H 
#define _GAMMA_EXPANSION_EFFECT_H 1

// An effect to convert the given gamma curve into linear light,
// typically inserted by the framework automatically at the beginning
// of the processing chain.
//
// Currently supports sRGB and Rec. 601/709.

#include "effect.h"
#include "effect_chain.h"

#define EXPANSION_CURVE_SIZE 256

class GammaExpansionEffect : public Effect {
private:
	// Should not be instantiated by end users.
	GammaExpansionEffect();
	friend class EffectChain;

public:
	virtual std::string effect_type_id() const { return "GammaExpansionEffect"; }
	std::string output_fragment_shader();

	virtual bool needs_linear_light() const { return false; }
	virtual bool needs_srgb_primaries() const { return false; }

	// Actually processes its input in a nonlinear fashion,
	// but does not touch alpha, and we are a special case anyway.
	virtual AlphaHandling alpha_handling() const { return DONT_CARE_ALPHA_TYPE; }

private:
	GammaCurve source_curve;
	float expansion_curve[EXPANSION_CURVE_SIZE];
};

#endif // !defined(_GAMMA_EXPANSION_EFFECT_H)
