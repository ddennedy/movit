#ifndef _MOVIT_MULTIPLY_EFFECT_H
#define _MOVIT_MULTIPLY_EFFECT_H 1

// An effect that multiplies every pixel by a constant (separate for each of
// R, G, B, A). A common use would be to reduce the alpha of an overlay before
// sending it through OverlayEffect, e.g. with R=G=B=A=0.3 to get 30% alpha
// (remember, alpha is premultiplied).

#include <epoxy/gl.h>
#include <string>

#include "effect.h"

namespace movit {

class MultiplyEffect : public Effect {
public:
	MultiplyEffect();
	virtual std::string effect_type_id() const { return "MultiplyEffect"; }
	std::string output_fragment_shader();
	virtual bool one_to_one_sampling() const { return true; }

private:
	RGBATuple factor;
};

}  // namespace movit

#endif // !defined(_MOVIT_MULTIPLY_EFFECT_H)
