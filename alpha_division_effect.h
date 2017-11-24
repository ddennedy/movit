#ifndef _MOVIT_ALPHA_DIVISION_EFFECT_H
#define _MOVIT_ALPHA_DIVISION_EFFECT_H 1

// Convert postmultiplied alpha to premultiplied alpha, simply by dividing.

#include <string>

#include "effect.h"

namespace movit {

class AlphaDivisionEffect : public Effect {
public:
	AlphaDivisionEffect() {}
	std::string effect_type_id() const override { return "AlphaDivisionEffect"; }
	std::string output_fragment_shader() override;
	bool strong_one_to_one_sampling() const override { return true; }
};

}  // namespace movit

#endif // !defined(_MOVIT_ALPHA_DIVISION_EFFECT_H)
