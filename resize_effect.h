#ifndef _RESIZE_EFFECT_H
#define _RESIZE_EFFECT_H 1

// An effect that simply resizes the picture to a given output size
// (set by the two integer parameters "width" and "height").
// Mostly useful as part of other algorithms.

#include "effect.h"

class ResizeEffect : public Effect {
public:
	ResizeEffect();
	virtual std::string effect_type_id() const { return "ResizeEffect"; }
	std::string output_fragment_shader();

	// We want processing done pre-filtering and mipmapped,
	// in case we need to scale down a lot.
	virtual bool need_texture_bounce() const { return true; }
	virtual bool needs_mipmaps() const { return true; }

	virtual bool changes_output_size() const { return true; }
	virtual void get_output_size(unsigned *width, unsigned *height) const;

private:
	int width, height;
};

#endif // !defined(_RESIZE_EFFECT_H)
