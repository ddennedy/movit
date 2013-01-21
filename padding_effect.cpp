#include <math.h>
#include <GL/glew.h>

#include "padding_effect.h"
#include "util.h"

PaddingEffect::PaddingEffect()
	: border_color(0.0f, 0.0f, 0.0f, 0.0f),
	  output_width(1280),
	  output_height(720),
	  top(0),
	  left(0)
{
	register_vec4("border_color", (float *)&border_color);
	register_int("width", &output_width);
	register_int("height", &output_height);
	register_float("top", &top);
	register_float("left", &left);
}

std::string PaddingEffect::output_fragment_shader()
{
	return read_file("padding_effect.frag");
}

void PaddingEffect::set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num)
{
	Effect::set_gl_state(glsl_program_num, prefix, sampler_num);

	float offset[2] = {
		left / output_width,
		(output_height - input_height - top) / output_height
	};
	set_uniform_vec2(glsl_program_num, prefix, "offset", offset);

	float scale[2] = {
		float(output_width) / input_width,
		float(output_height) / input_height
	};
	set_uniform_vec2(glsl_program_num, prefix, "scale", scale);

	// Due to roundoff errors, the test against 0.5 is seldom exact,
	// even though we test for less than and not less-than-or-equal.
	// We'd rather keep an extra border pixel in those very rare cases
	// (where the image is shifted pretty much exactly a half-pixel)
	// than losing a pixel in the common cases of integer shift.
	// Thus the 1e-3 fudge factors.
	float texcoord_min[2] = {
		float((0.5f - 1e-3) / input_width),
		float((0.5f - 1e-3) / input_height)
	};
	set_uniform_vec2(glsl_program_num, prefix, "texcoord_min", texcoord_min);

	float texcoord_max[2] = {
		float(1.0f - (0.5f - 1e-3) / input_width),
		float(1.0f - (0.5f - 1e-3) / input_height)
	};
	set_uniform_vec2(glsl_program_num, prefix, "texcoord_max", texcoord_max);
}
	
// We don't change the pixels of the image itself, so the only thing that 
// can make us less flexible is if the border color can be interpreted
// differently in different modes.

// 0.0 and 1.0 are interpreted the same, no matter the gamma ramp.
// Alpha is not affected by gamma.
bool PaddingEffect::needs_linear_light() const
{
	if ((border_color.r == 0.0 || border_color.r == 1.0) &&
	    (border_color.g == 0.0 || border_color.g == 1.0) &&
	    (border_color.b == 0.0 || border_color.b == 1.0)) {
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

// If the border color is black, it doesn't matter if we're pre- or postmultiplied
// (or even blank, as a hack). Otherwise, it does.
Effect::AlphaHandling PaddingEffect::alpha_handling() const
{
	if (border_color.r == 0.0 && border_color.g == 0.0 && border_color.b == 0.0) {
		return DONT_CARE_ALPHA_TYPE;
	}
	return INPUT_AND_OUTPUT_ALPHA_PREMULTIPLIED;
}
	
void PaddingEffect::get_output_size(unsigned *width, unsigned *height) const
{
	*width = output_width;
	*height = output_height;
}
	
void PaddingEffect::inform_input_size(unsigned input_num, unsigned width, unsigned height)
{
	assert(input_num == 0);
	input_width = width;
	input_height = height;
}
