#ifndef _COLORSPACE_CONVERSION_EFFECT_H
#define _COLORSPACE_CONVERSION_EFFECT_H 1

// An effect to convert between different color spaces.
// Can convert freely between sRGB/Rec. 709 and the two different Rec. 601
// color spaces (which thankfully have the same white point).
//
// We don't do any fancy gamut mapping or similar; colors that are out-of-gamut
// will simply stay out-of-gamut, and probably clip in the output stage.

#include "effect.h"
#include "effect_chain.h"

class ColorspaceConversionEffect : public Effect {
public:
	ColorspaceConversionEffect();
	virtual std::string effect_type_id() const { return "ColorspaceConversionEffect"; }
	std::string output_fragment_shader();

	virtual bool needs_srgb_primaries() const { return false; }
	virtual AlphaHandling alpha_handling() const { return DONT_CARE_ALPHA_TYPE; }

private:
	Colorspace source_space, destination_space;
};

#endif // !defined(_COLORSPACE_CONVERSION_EFFECT_H)
