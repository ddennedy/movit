#include <epoxy/gl.h>

#include "effect_chain.h"
#include "slice_effect.h"
#include "effect_util.h"
#include "util.h"

using namespace std;

namespace movit {

SliceEffect::SliceEffect()
	: input_slice_size(1),
	  output_slice_size(1),
	  offset(0),
	  direction(VERTICAL)
{
	register_int("input_slice_size", &input_slice_size);
	register_int("output_slice_size", &output_slice_size);
	register_int("offset", &offset);
	register_int("direction", (int *)&direction);
	register_uniform_float("output_coord_to_slice_num", &uniform_output_coord_to_slice_num);
	register_uniform_float("slice_num_to_input_coord", &uniform_slice_num_to_input_coord);
	register_uniform_float("slice_offset_to_input_coord", &uniform_slice_offset_to_input_coord);
	register_uniform_float("normalized_offset", &uniform_offset);
}

string SliceEffect::output_fragment_shader()
{
	char buf[256];
	sprintf(buf, "#define DIRECTION_VERTICAL %d\n", (direction == VERTICAL));
	return buf + read_file("slice_effect.frag");
}
	
void SliceEffect::inform_input_size(unsigned input_num, unsigned width, unsigned height)
{
	assert(input_num == 0);
	input_width = width;
	input_height = height;
}

void SliceEffect::get_output_size(unsigned *width, unsigned *height,
                                  unsigned *virtual_width, unsigned *virtual_height) const
{
	if (direction == HORIZONTAL) {
		*width = div_round_up(input_width, input_slice_size) * output_slice_size;
		*height = input_height;	
	} else {
		*width = input_width;	
		*height = div_round_up(input_height, input_slice_size) * output_slice_size;
	}
	*virtual_width = *width;
	*virtual_height = *height;
}

void SliceEffect::set_gl_state(GLuint glsl_program_num, const string &prefix, unsigned *sampler_num)
{
	Effect::set_gl_state(glsl_program_num, prefix, sampler_num);

	unsigned output_width, output_height;
	get_output_size(&output_width, &output_height, &output_width, &output_height);

	if (direction == HORIZONTAL) {
		uniform_output_coord_to_slice_num = float(output_width) / float(output_slice_size);
		uniform_slice_num_to_input_coord = float(input_slice_size) / float(input_width);
		uniform_slice_offset_to_input_coord = float(output_slice_size) / float(input_width);
		uniform_offset = float(offset) / float(input_width);
	} else {
		uniform_output_coord_to_slice_num = float(output_height) / float(output_slice_size);
		uniform_slice_num_to_input_coord = float(input_slice_size) / float(input_height);
		uniform_slice_offset_to_input_coord = float(output_slice_size) / float(input_height);
		uniform_offset = float(offset) / float(input_height);
	}

	// Normalized coordinates could potentially cause blurring of the image.
	// It isn't critical, but still good practice.
	Node *self = chain->find_node_for_effect(this);
	glActiveTexture(chain->get_input_sampler(self, 0));
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	check_error();
}

}  // namespace movit
