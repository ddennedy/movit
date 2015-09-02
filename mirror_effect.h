#ifndef _MOVIT_MIRROR_EFFECT_H
#define _MOVIT_MIRROR_EFFECT_H 1

// A simple horizontal mirroring.

#include <string>

#include "effect.h"

namespace movit {

class MirrorEffect : public Effect {
public:
	MirrorEffect();
	virtual std::string effect_type_id() const { return "MirrorEffect"; }
	std::string output_fragment_shader();

	virtual bool needs_linear_light() const { return false; }
	virtual bool needs_srgb_primaries() const { return false; }
	virtual AlphaHandling alpha_handling() const { return DONT_CARE_ALPHA_TYPE; }
	virtual bool one_to_one_sampling() const { return true; }
};

}  // namespace movit

#endif // !defined(_MOVIT_MIRROR_EFFECT_H)
