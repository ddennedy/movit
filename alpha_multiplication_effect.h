#ifndef _ALPHA_MULTIPLICATION_EFFECT_H
#define _ALPHA_MULTIPLICATION_EFFECT_H 1

// Convert postmultiplied alpha to premultiplied alpha, simply by multiplying.

#include "effect.h"

class AlphaMultiplicationEffect : public Effect {
public:
	AlphaMultiplicationEffect() {}
	virtual std::string effect_type_id() const { return "AlphaMultiplicationEffect"; }
	std::string output_fragment_shader();
};

#endif // !defined(_ALPHA_MULTIPLICATION_EFFECT_H)
