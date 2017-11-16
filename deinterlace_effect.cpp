#include <epoxy/gl.h>

#include "deinterlace_effect.h"
#include "effect_chain.h"
#include "init.h"
#include "util.h"

using namespace std;

namespace movit {

DeinterlaceEffect::DeinterlaceEffect()
	: enable_spatial_interlacing_check(true),
	  current_field_position(TOP),
	  num_lines(1080)
{
	if (movit_compute_shaders_supported) {
		compute_effect_owner.reset(new DeinterlaceComputeEffect);
		compute_effect = compute_effect_owner.get();
	} else {
		register_int("enable_spatial_interlacing_check", (int *)&enable_spatial_interlacing_check);
		register_int("current_field_position", (int *)&current_field_position);
		register_uniform_float("num_lines", &num_lines);
		register_uniform_float("inv_width", &inv_width);
		register_uniform_float("self_offset", &self_offset);
		register_uniform_float_array("current_offset", current_offset, 2);
		register_uniform_float_array("other_offset", other_offset, 3);
	}
}

string DeinterlaceEffect::output_fragment_shader()
{
	char buf[256];
	snprintf(buf, sizeof(buf), "#define YADIF_ENABLE_SPATIAL_INTERLACING_CHECK %d\n",
		enable_spatial_interlacing_check);
	string frag_shader = buf;

	frag_shader += read_file("deinterlace_effect.frag");
	return frag_shader;
}

void DeinterlaceEffect::rewrite_graph(EffectChain *graph, Node *self)
{
	if (compute_effect != nullptr) {
		Node *compute_node = graph->add_node(compute_effect_owner.release());
		graph->replace_receiver(self, compute_node);
		graph->replace_sender(self, compute_node);
		self->disabled = true;
	}
}

bool DeinterlaceEffect::set_int(const std::string &key, int value)
{
	if (compute_effect != nullptr) {
		return compute_effect->set_int(key, value);
	} else {
		return Effect::set_int(key, value);
	}
}

void DeinterlaceEffect::inform_input_size(unsigned input_num, unsigned width, unsigned height)
{
	assert(input_num >= 0 && input_num < 5);
	widths[input_num] = width;
	heights[input_num] = height;
	num_lines = height * 2;
}

void DeinterlaceEffect::get_output_size(unsigned *width, unsigned *height,
                                        unsigned *virtual_width, unsigned *virtual_height) const
{
	assert(widths[0] == widths[1]);
	assert(widths[1] == widths[2]);
	assert(widths[2] == widths[3]);
	assert(widths[3] == widths[4]);
	assert(heights[0] == heights[1]);
	assert(heights[1] == heights[2]);
	assert(heights[2] == heights[3]);
	assert(heights[3] == heights[4]);
	*width = *virtual_width = widths[0];
	*height = *virtual_height = heights[0] * 2;
}

void DeinterlaceEffect::set_gl_state(GLuint glsl_program_num, const string &prefix, unsigned *sampler_num)
{
	Effect::set_gl_state(glsl_program_num, prefix, sampler_num);

	inv_width = 1.0 / widths[0];

	// Texel centers: t = output texel center for top field, b = for bottom field,
	// x = the input texel. (The same area is two pixels for output, one for input;
	// thus the stippled line in the middle.)
	//
	// +---------+
	// |         |
	// |    t    |
	// |         |
	// | - -x- - |
	// |         |
	// |    b    |
	// |         |
	// +---------+
	//
	// Note as usual OpenGL's bottom-left convention.
	if (current_field_position == 0) {
		// Top.
		self_offset = -0.5 / num_lines;
	} else {
		// Bottom.
		assert(current_field_position == 1);
		self_offset = 0.5 / num_lines;
	}

	// Having now established where the texels lie for the uninterpolated samples,
	// we can use that to figure out where to sample for the interpolation. Drawing
	// the fields as what lines they represent, here for three-pixel high fields
	// with current_field_position == 0 (plus an “o” to mark the pixel we're trying
	// to interpolate, and “c” for corresponding texel in the other field):
	//
	// Prev Cur Next
	//       x
	//   x       x
	//       x
	//   c   o   c
	//       x
	//   x       x
	//
	// Obviously, for sampling in the current field, we are one half-texel off
	// compared to <self_offset>, so sampling in the current field is easy:
	current_offset[0] = self_offset - 0.5 / heights[0];
	current_offset[1] = self_offset + 0.5 / heights[0];

	// Now to find the texel in the other fields corresponding to the pixel
	// we're trying to interpolate, let's realign the diagram above:
	//
	// Prev Cur Next
	//   x   x   x
	//
	//   c   x   c
	//       o
	//   x   x   x
	//
	// So obviously for this case, we need to center on the same place as
	// current_offset[1] (the texel directly above the o; note again the
	// bottom-left convention). For the case of current_field_position == 1,
	// the shift in the alignment goes the other way, and what we want
	// is current_offset[0] (the texel directly below the o).
	float center_offset = current_offset[1 - current_field_position];
	other_offset[0] = center_offset - 1.0 / heights[0];
	other_offset[1] = center_offset;
	other_offset[2] = center_offset + 1.0 / heights[0];
}

// Implementation of DeinterlaceComputeEffect.

DeinterlaceComputeEffect::DeinterlaceComputeEffect()
	: enable_spatial_interlacing_check(true),
	  current_field_position(TOP)
{
	register_int("enable_spatial_interlacing_check", (int *)&enable_spatial_interlacing_check);
	register_int("current_field_position", (int *)&current_field_position);
	register_uniform_float("inv_width", &inv_width);
	register_uniform_float("inv_height", &inv_height);
	register_uniform_float("current_field_vertical_offset", &current_field_vertical_offset);
}

string DeinterlaceComputeEffect::output_fragment_shader()
{
	char buf[256];
	snprintf(buf, sizeof(buf), "#define YADIF_ENABLE_SPATIAL_INTERLACING_CHECK %d\n",
		enable_spatial_interlacing_check);
	string frag_shader = buf;

	frag_shader += read_file("deinterlace_effect.comp");
	return frag_shader;
}

void DeinterlaceComputeEffect::inform_input_size(unsigned input_num, unsigned width, unsigned height)
{
	assert(input_num >= 0 && input_num < 5);
	widths[input_num] = width;
	heights[input_num] = height;
}

void DeinterlaceComputeEffect::get_output_size(unsigned *width, unsigned *height,
                                        unsigned *virtual_width, unsigned *virtual_height) const
{
	assert(widths[0] == widths[1]);
	assert(widths[1] == widths[2]);
	assert(widths[2] == widths[3]);
	assert(widths[3] == widths[4]);
	assert(heights[0] == heights[1]);
	assert(heights[1] == heights[2]);
	assert(heights[2] == heights[3]);
	assert(heights[3] == heights[4]);
	*width = *virtual_width = widths[0];
	*height = *virtual_height = heights[0] * 2;
}

void DeinterlaceComputeEffect::set_gl_state(GLuint glsl_program_num, const string &prefix, unsigned *sampler_num)
{
	Effect::set_gl_state(glsl_program_num, prefix, sampler_num);

	inv_width = 1.0 / widths[0];
	inv_height = 1.0 / heights[0];

	// For the compute shader, we need to load a block of pixels. Marking off the
	// ones we are supposed to interpolate (looking only at one column):
	//
	//  field_pos==0            field_pos==1
	//
	//  6     x      ↑          6     .      ↑
	//  6     .      |          6     x      |
	//  5     x      |          5     .      |
	//  5     .      |          5     x      |
	//  4     x      |          4     .      |
	//  4     .      |          4     x      |
	//  3     x      | y        3     o      | y
	//  3     o      |          3     x      |
	//  2     x      |          2     o      |
	//  2     o      |          2     x      |
	//  1     x      |          1     .      |
	//  1     .      |          1     x      |
	//  0     x      |          0     .      |
	//  0     .      |          0     x      |
	//
	// So if we are to compute e.g. output samples [2,4), we load input samples
	// [1,3] for TFF and samples [2,4] for BFF.
	if (current_field_position == 0) {
		current_field_vertical_offset = -1.0 / heights[0];
	} else {
		current_field_vertical_offset =  0.0 / heights[0];
	}
}

void DeinterlaceComputeEffect::get_compute_dimensions(unsigned output_width, unsigned output_height,
                                               unsigned *x, unsigned *y, unsigned *z) const
{
	// Each workgroup outputs 8x32 pixels (see GROUP_W and GROUP_H in the shader),
	// so figure out the number of groups by simply rounding up.
	*x = (output_width + 7) / 8;
	*y = (output_height + 31) / 32;
	*z = 1;
}

}  // namespace movit
