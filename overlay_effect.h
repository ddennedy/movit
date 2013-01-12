#ifndef _OVERLAY_EFFECT_H
#define _OVERLAY_EFFECT_H 1

// Put one image on top of another, using alpha where appropriate.
// (If both images are the same aspect and the top image has alpha=1.0
// for all pixels, you will not see anything of the one on the bottom.)
//
// This is the “atop” operation from Porter-Duff blending, also used
// when merging layers in e.g. GIMP or Photoshop.
//
// The first input is the bottom, and the second is the top.

#include "effect.h"

class OverlayEffect : public Effect {
public:
	OverlayEffect();
	virtual std::string effect_type_id() const { return "OverlayEffect"; }
	std::string output_fragment_shader();

	virtual bool needs_srgb_primaries() const { return false; }
	virtual unsigned num_inputs() const { return 2; }
};

#endif // !defined(_OVERLAY_EFFECT_H)
