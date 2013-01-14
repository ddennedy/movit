#ifndef _MIX_EFFECT_H
#define _MIX_EFFECT_H 1

// Combine two images: a*x + b*y. If you set a within [0,1] and b=1-a,
// you will get a fade; if not, you may get surprising results (consider alpha).

#include "effect.h"

class MixEffect : public Effect {
public:
	MixEffect();
	virtual std::string effect_type_id() const { return "MixEffect"; }
	std::string output_fragment_shader();

	virtual bool needs_srgb_primaries() const { return false; }
	virtual unsigned num_inputs() const { return 2; }

private:
	float strength_first, strength_second;
};

#endif // !defined(_MIX_EFFECT_H)
