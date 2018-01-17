#ifndef _MOVIT_RESIZE_EFFECT_H
#define _MOVIT_RESIZE_EFFECT_H 1

// An effect that simply resizes the picture to a given output size
// (set by the two integer parameters "width" and "height").
// Mostly useful as part of other algorithms.

#include <string>

#include "effect.h"

namespace movit {

class ResizeEffect : public Effect {
public:
	ResizeEffect();
	std::string effect_type_id() const override { return "ResizeEffect"; }
	std::string output_fragment_shader() override;

	// We want processing done pre-filtering and mipmapped,
	// in case we need to scale down a lot.
	bool needs_texture_bounce() const override { return true; }
	MipmapRequirements needs_mipmaps() const override { return NEEDS_MIPMAPS; }
	AlphaHandling alpha_handling() const override { return INPUT_PREMULTIPLIED_ALPHA_KEEP_BLANK; }

	bool changes_output_size() const override { return true; }
	bool sets_virtual_output_size() const override { return false; }
	void get_output_size(unsigned *width, unsigned *height, unsigned *virtual_width, unsigned *virtual_height) const override;

private:
	int width, height;
};

}  // namespace movit

#endif // !defined(_MOVIT_RESIZE_EFFECT_H)
