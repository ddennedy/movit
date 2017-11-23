#ifndef _MOVIT_MIRROR_EFFECT_H
#define _MOVIT_MIRROR_EFFECT_H 1

// A simple horizontal mirroring.

#include <string>

#include "effect.h"

namespace movit {

class MirrorEffect : public Effect {
public:
	MirrorEffect();
	std::string effect_type_id() const override { return "MirrorEffect"; }
	std::string output_fragment_shader() override;

	bool needs_linear_light() const override { return false; }
	bool needs_srgb_primaries() const override { return false; }
	AlphaHandling alpha_handling() const override { return DONT_CARE_ALPHA_TYPE; }
	bool one_to_one_sampling() const override { return true; }
};

}  // namespace movit

#endif // !defined(_MOVIT_MIRROR_EFFECT_H)
