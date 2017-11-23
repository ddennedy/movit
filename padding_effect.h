#ifndef _MOVIT_PADDING_EFFECT_H
#define _MOVIT_PADDING_EFFECT_H 1

// Takes an image and pads it to fit a larger image, or crops it to fit a smaller one
// (although the latter is implemented slightly less efficiently, and you cannot both
// pad and crop in the same effect).
//
// The source image is cut off at the texture border, and then given a user-specific color;
// by default, full transparent. You can give a fractional border size (non-integral
// "top" or "left" offset) if you wish, which will give you linear interpolation of
// both pixel data of and the border. Furthermore, you can offset where the border falls
// by using the "border_offset_{top,bottom,left,right}" settings; this is particularly
// useful if you use ResampleEffect earlier in the chain for high-quality fractional-pixel
// translation and just want PaddingEffect to get the border right.
//
// The border color is taken to be in linear gamma, sRGB, with premultiplied alpha.
// You may not change it after calling finalize(), since that could change the
// graph (need_linear_light() etc. depend on the border color you choose).
//
// IntegralPaddingEffect is like PaddingEffect, except that "top" and "left" parameters
// are int parameters instead of float. This allows it to guarantee one-to-one sampling,
// which can speed up processing by allowing more effect passes to be collapsed.
// border_offset_* are still allowed to be float, although you should beware that if
// you set e.g. border_offset_top to a negative value, you will be sampling outside
// the edge and will read data that is undefined in one-to-one-mode (could be
// edge repeat, could be something else). With regular PaddingEffect, such samples
// are guaranteed to be edge repeat.

#include <epoxy/gl.h>
#include <string>

#include "effect.h"

namespace movit {

class PaddingEffect : public Effect {
public:
	PaddingEffect();
	std::string effect_type_id() const override { return "PaddingEffect"; }
	std::string output_fragment_shader() override;
	void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num) override;

	bool needs_linear_light() const override;
	bool needs_srgb_primaries() const override;
	AlphaHandling alpha_handling() const override;
	
	bool changes_output_size() const override { return true; }
	bool sets_virtual_output_size() const override { return false; }
	void get_output_size(unsigned *width, unsigned *height, unsigned *virtual_width, unsigned *virtual_height) const override;
	void inform_input_size(unsigned input_num, unsigned width, unsigned height) override;

private:
	RGBATuple border_color;
	int input_width, input_height;
	int output_width, output_height;
	float top, left;
	float border_offset_top, border_offset_left;
	float border_offset_bottom, border_offset_right;
	float uniform_offset[2], uniform_scale[2];
	float uniform_normalized_coords_to_texels[2];
	float uniform_offset_bottomleft[2], uniform_offset_topright[2];
};

class IntegralPaddingEffect : public PaddingEffect {
public:
	IntegralPaddingEffect();
	std::string effect_type_id() const override { return "IntegralPaddingEffect"; }
	bool one_to_one_sampling() const override { return true; }
	bool set_int(const std::string&, int value) override;
	bool set_float(const std::string &key, float value) override;
};

}  // namespace movit

#endif // !defined(_MOVIT_PADDING_EFFECT_H)
