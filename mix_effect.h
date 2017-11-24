#ifndef _MOVIT_MIX_EFFECT_H
#define _MOVIT_MIX_EFFECT_H 1

// Combine two images: a*x + b*y. If you set a within [0,1] and b=1-a,
// you will get a fade; if not, you may get surprising results (consider alpha).

#include <string>

#include "effect.h"

namespace movit {

class MixEffect : public Effect {
public:
	MixEffect();
	std::string effect_type_id() const override { return "MixEffect"; }
	std::string output_fragment_shader() override;

	bool needs_srgb_primaries() const override { return false; }
	unsigned num_inputs() const override { return 2; }
	bool strong_one_to_one_sampling() const override { return true; }

	// TODO: In the common case where a+b=1, it would be useful to be able to set
	// alpha_handling() to INPUT_PREMULTIPLIED_ALPHA_KEEP_BLANK. However, right now
	// we have no way of knowing that at instantiation time.

private:
	float strength_first, strength_second;
};

}  // namespace movit

#endif // !defined(_MOVIT_MIX_EFFECT_H)
