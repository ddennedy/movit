#ifndef _MOVIT_ALPHA_DIVISION_EFFECT_H
#define _MOVIT_ALPHA_DIVISION_EFFECT_H 1

// Convert postmultiplied alpha to premultiplied alpha, simply by dividing.

#include <string>

#include "effect.h"

namespace movit {

class AlphaDivisionEffect : public Effect {
public:
	AlphaDivisionEffect() {}
	virtual std::string effect_type_id() const { return "AlphaDivisionEffect"; }
	std::string output_fragment_shader();
	virtual bool one_to_one_sampling() const { return true; }
};

}  // namespace movit

#endif // !defined(_MOVIT_ALPHA_DIVISION_EFFECT_H)
