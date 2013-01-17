#ifndef _PADDING_EFFECT_H
#define _PADDING_EFFECT_H 1

// Takes an image and pads it to fit a larger image, or crops it to fit a smaller one
// (although the latter is implemented slightly less efficiently, and you cannot both
// pad and crop in the same effect).
//
// The source image is cut off at the texel borders (so there is no interpolation
// outside them), and then given a user-specific color; by default, full transparent.
//
// The border color is taken to be in linear gamma, sRGB, with premultiplied alpha.
// You may not change it after calling finalize(), since that could change the
// graph (need_linear_light() etc. depend on the border color you choose).

#include "effect.h"

class PaddingEffect : public Effect {
public:
	PaddingEffect();
	virtual std::string effect_type_id() const { return "PaddingEffect"; }
	std::string output_fragment_shader();
	void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num);

	virtual bool needs_linear_light() const;
	virtual bool needs_srgb_primaries() const;
	virtual AlphaHandling alpha_handling() const;
	
	virtual bool changes_output_size() const { return true; }
	virtual void get_output_size(unsigned *width, unsigned *height) const;
	virtual void inform_input_size(unsigned input_num, unsigned width, unsigned height);

private:
	RGBATriplet border_color;
	int input_width, input_height;
	int output_width, output_height;
	float top, left;
};

#endif // !defined(_PADDING_EFFECT_H)
