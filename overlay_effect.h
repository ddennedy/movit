#ifndef _MOVIT_OVERLAY_EFFECT_H
#define _MOVIT_OVERLAY_EFFECT_H 1

// Put one image on top of another, using various blending modes.
// Supports all Qt QPainter::CompositionMode blend modes including
// Porter-Duff operations and SVG 1.2 blend modes.
//
// The first input is the bottom, and the second is the top.

#include <string>

#include "effect.h"

namespace movit {

class OverlayEffect : public Effect {
public:
	// Blend modes matching Qt QPainter::CompositionMode
	enum BlendMode {
		// Porter-Duff modes
		BLEND_MODE_SOURCE_OVER,
		BLEND_MODE_DESTINATION_OVER,
		BLEND_MODE_CLEAR,
		BLEND_MODE_SOURCE,
		BLEND_MODE_DESTINATION,
		BLEND_MODE_SOURCE_IN,
		BLEND_MODE_DESTINATION_IN,
		BLEND_MODE_SOURCE_OUT,
		BLEND_MODE_DESTINATION_OUT,
		BLEND_MODE_SOURCE_ATOP,
		BLEND_MODE_DESTINATION_ATOP,
		BLEND_MODE_XOR,
		
		// SVG 1.2 blend modes
		BLEND_MODE_PLUS,
		BLEND_MODE_MULTIPLY,
		BLEND_MODE_SCREEN,
		BLEND_MODE_OVERLAY,
		BLEND_MODE_DARKEN,
		BLEND_MODE_LIGHTEN,
		BLEND_MODE_COLOR_DODGE,
		BLEND_MODE_COLOR_BURN,
		BLEND_MODE_HARD_LIGHT,
		BLEND_MODE_SOFT_LIGHT,
		BLEND_MODE_DIFFERENCE,
		BLEND_MODE_EXCLUSION
	};

	OverlayEffect();
	std::string effect_type_id() const override { return "OverlayEffect"; }
	std::string output_fragment_shader() override;

	bool needs_srgb_primaries() const override { return false; }
	unsigned num_inputs() const override { return 2; }
	bool strong_one_to_one_sampling() const override { return true; }

	// Actually, if _either_ image has blank alpha, our output will have
	// blank alpha, too (this only tells the framework that having _both_
	// images with blank alpha would result in blank alpha).
	// However, understanding that would require changes
	// to EffectChain, so postpone that optimization for later.
	AlphaHandling alpha_handling() const override { return INPUT_PREMULTIPLIED_ALPHA_KEEP_BLANK; }

private:
	// If true, overlays input1 on top of input2 instead of vice versa.
	// Must be set before finalize.
	bool swap_inputs;
	
	// Blend mode to use (default is source over)
	int blend_mode;
};

}  // namespace movit

#endif // !defined(_MOVIT_OVERLAY_EFFECT_H)
