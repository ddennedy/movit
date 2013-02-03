#ifndef _MOVIT_ALPHA_DIVISION_EFFECT_H
#define _MOVIT_ALPHA_DIVISION_EFFECT_H 1

// Convert premultiplied alpha to postmultiplied alpha, simply by multiplying.

#include <string>

#include "effect.h"

class AlphaDivisionEffect : public Effect {
public:
	AlphaDivisionEffect() {}
	virtual std::string effect_type_id() const { return "AlphaDivisionEffect"; }
	std::string output_fragment_shader();
};

#endif // !defined(_MOVIT_ALPHA_DIVISION_EFFECT_H)
