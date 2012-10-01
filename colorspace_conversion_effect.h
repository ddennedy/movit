#ifndef _COLORSPACE_CONVERSION_EFFECT_H
#define _COLORSPACE_CONVERSION_EFFECT_H 1

#include "effect.h"
#include "effect_chain.h"

class ColorSpaceConversionEffect : public Effect {
public:
	ColorSpaceConversionEffect();

private:
	ColorSpace source_space, destination_space;
};

#endif // !defined(_COLORSPACE_CONVERSION_EFFECT_H)
