#ifndef _MIRROR_EFFECT_H
#define _MIRROR_EFFECT_H 1

// A simple horizontal mirroring.

#include "effect.h"

class MirrorEffect : public Effect {
public:
	MirrorEffect();
	virtual std::string effect_type_id() const { return "MirrorEffect"; }
	std::string output_fragment_shader();

	virtual bool needs_linear_light() const { return false; }
	virtual bool needs_srgb_primaries() const { return false; }
	virtual AlphaHandling alpha_handling() const { return DONT_CARE_ALPHA_TYPE; }
};

#endif // !defined(_MIRROR_EFFECT_H)
