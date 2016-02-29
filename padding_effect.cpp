#include <epoxy/gl.h>
#include <assert.h>

#include "effect_util.h"
#include "padding_effect.h"
#include "util.h"

using namespace std;

namespace movit {

PaddingEffect::PaddingEffect()
	: border_color(0.0f, 0.0f, 0.0f, 0.0f),
	  output_width(1280),
	  output_height(720),
	  top(0),
	  left(0),
	  border_offset_top(0.0f),
	  border_offset_left(0.0f),
	  border_offset_bottom(0.0f),
	  border_offset_right(0.0f)
{
	register_vec4("border_color", (float *)&border_color);
	register_int("width", &output_width);
	register_int("height", &output_height);
	register_float("top", &top);
	register_float("left", &left);
	register_float("border_offset_top", &border_offset_top);
	register_float("border_offset_left", &border_offset_left);
	register_float("border_offset_bottom", &border_offset_bottom);
	register_float("border_offset_right", &border_offset_right);
	register_uniform_vec2("offset", uniform_offset);
	register_uniform_vec2("scale", uniform_scale);
	register_uniform_vec2("normalized_coords_to_texels", uniform_normalized_coords_to_texels);
	register_uniform_vec2("offset_bottomleft", uniform_offset_bottomleft);
	register_uniform_vec2("offset_topright", uniform_offset_topright);
}

string PaddingEffect::output_fragment_shader()
{
	return read_file("padding_effect.frag");
}

void PaddingEffect::set_gl_state(GLuint glsl_program_num, const string &prefix, unsigned *sampler_num)
{
	Effect::set_gl_state(glsl_program_num, prefix, sampler_num);

	uniform_offset[0] = left / output_width;
	uniform_offset[1] = (output_height - input_height - top) / output_height;

	uniform_scale[0] = float(output_width) / input_width;
	uniform_scale[1] = float(output_height) / input_height;

	uniform_normalized_coords_to_texels[0] = float(input_width);
	uniform_normalized_coords_to_texels[1] = float(input_height);

	// Texels -0.5..0.5 should map to light level 0..1 (and then we
	// clamp the rest).
	uniform_offset_bottomleft[0] = 0.5f - border_offset_left;
	uniform_offset_bottomleft[1] = 0.5f + border_offset_bottom;

	// Texels size-0.5..size+0.5 should map to light level 1..0 (and then clamp).
	uniform_offset_topright[0] = input_width + 0.5f + border_offset_right;
	uniform_offset_topright[1] = input_height + 0.5f - border_offset_top;
}
	
// We don't change the pixels of the image itself, so the only thing that 
// can make us less flexible is if the border color can be interpreted
// differently in different modes.

// 0.0 and 1.0 are interpreted the same, no matter the gamma ramp.
// Alpha is not affected by gamma per se, but the combination of
// premultiplied alpha and non-linear gamma curve does not make sense,
// so if could possibly be converting blank alpha to non-blank
// (ie., premultiplied), we need our output to be in linear light.
bool PaddingEffect::needs_linear_light() const
{
	if ((border_color.r == 0.0 || border_color.r == 1.0) &&
	    (border_color.g == 0.0 || border_color.g == 1.0) &&
	    (border_color.b == 0.0 || border_color.b == 1.0) &&
	    border_color.a == 1.0) {
		return false;
	}
	return true;
}

// The white point is the same (D65) in all the color spaces we currently support,
// so any gray would be okay, but we don't really have a guarantee for that.
// Stay safe and say that only pure black and pure white is okay.
// Alpha is not affected by color space.
bool PaddingEffect::needs_srgb_primaries() const
{
	if (border_color.r == 0.0 && border_color.g == 0.0 && border_color.b == 0.0) {
		return false;
	}
	if (border_color.r == 1.0 && border_color.g == 1.0 && border_color.b == 1.0) {
		return false;
	}
	return true;
}

Effect::AlphaHandling PaddingEffect::alpha_handling() const
{
	// If the border color is solid, it doesn't matter if we're pre- or postmultiplied.
	if (border_color.a == 1.0) {
		return DONT_CARE_ALPHA_TYPE;
	}

	// Otherwise, we're going to output our border color in premultiplied alpha,
	// so the other pixels better be premultiplied as well.
	// Note that for non-solid black (i.e. alpha < 1.0), we're equally fine with
	// pre- and postmultiplied, but we are _not_ fine with blank being passed through,
	// and we don't have a way to specify that.
	return INPUT_AND_OUTPUT_PREMULTIPLIED_ALPHA;
}
	
void PaddingEffect::get_output_size(unsigned *width, unsigned *height, unsigned *virtual_width, unsigned *virtual_height) const
{
	*virtual_width = *width = output_width;
	*virtual_height = *height = output_height;
}
	
void PaddingEffect::inform_input_size(unsigned input_num, unsigned width, unsigned height)
{
	assert(input_num == 0);
	input_width = width;
	input_height = height;
}

IntegralPaddingEffect::IntegralPaddingEffect() {}

bool IntegralPaddingEffect::set_int(const std::string &key, int value)
{
	if (key == "top" || key == "left") {
		return PaddingEffect::set_float(key, value);
	} else {
		return PaddingEffect::set_int(key, value);
	}
}

bool IntegralPaddingEffect::set_float(const std::string &key, float value)
{
	if (key == "top" || key == "left") {
		// These are removed as float parameters from this version.
		return false;
	} else {
		return PaddingEffect::set_float(key, value);
	}
}

}  // namespace movit
