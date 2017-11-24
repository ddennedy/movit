#ifndef _MOVIT_COLORSPACE_CONVERSION_EFFECT_H
#define _MOVIT_COLORSPACE_CONVERSION_EFFECT_H 1

// An effect to convert between different color spaces.
// Can convert freely between sRGB/Rec. 709 and the two different Rec. 601
// color spaces (which thankfully have the same white point).
//
// We don't do any fancy gamut mapping or similar; colors that are out-of-gamut
// will simply stay out-of-gamut, and probably clip in the output stage.

#include <Eigen/Core>
#include <string>

#include "effect.h"
#include "image_format.h"

namespace movit {

class ColorspaceConversionEffect : public Effect {
private:
	// Should not be instantiated by end users.
	ColorspaceConversionEffect();
	friend class EffectChain;

public:
	std::string effect_type_id() const override { return "ColorspaceConversionEffect"; }
	std::string output_fragment_shader() override;

	bool needs_srgb_primaries() const override { return false; }
	AlphaHandling alpha_handling() const override { return DONT_CARE_ALPHA_TYPE; }
	bool strong_one_to_one_sampling() const override { return true; }

	// Get a conversion matrix from the given color space to XYZ.
	static Eigen::Matrix3d get_xyz_matrix(Colorspace space);

private:
	Colorspace source_space, destination_space;
};

}  // namespace movit

#endif // !defined(_MOVIT_COLORSPACE_CONVERSION_EFFECT_H)
