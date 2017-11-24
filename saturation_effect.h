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
	std::string effect_type_id() const override { return "SaturationEffect"; }
	AlphaHandling alpha_handling() const override { return DONT_CARE_ALPHA_TYPE; }
	bool strong_one_to_one_sampling() const override { return true; }
	std::string output_fragment_shader() override;

private:
	float saturation;
};

}  // namespace movit

#endif // !defined(_MOVIT_SATURATION_EFFECT_H)
