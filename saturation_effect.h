#ifndef _MOVIT_SATURATION_EFFECT_H
#define _MOVIT_SATURATION_EFFECT_H 1

// A simple desaturation/saturation effect. We use the Rec. 709
// definition of luminance (in linear light, of course) and linearly
// interpolate between that (saturation=0) and the original signal
// (saturation=1). Extrapolating that curve further (ie., saturation > 1)
// gives us increased saturation if so desired.

#include <string>

#include "effect.h"

namespace movit {

class SaturationEffect : public Effect {
public:
	SaturationEffect();
	virtual std::string effect_type_id() const { return "SaturationEffect"; }
	virtual AlphaHandling alpha_handling() const { return DONT_CARE_ALPHA_TYPE; }
	virtual bool one_to_one_sampling() const { return true; }
	std::string output_fragment_shader();

private:
	float saturation;
};

}  // namespace movit

#endif // !defined(_MOVIT_SATURATION_EFFECT_H)
