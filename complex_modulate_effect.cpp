#include <epoxy/gl.h>

#include "complex_modulate_effect.h"
#include "effect_chain.h"
#include "effect_util.h"
#include "util.h"

using namespace std;

namespace movit {

ComplexModulateEffect::ComplexModulateEffect()
	: num_repeats_x(1), num_repeats_y(1)
{
	register_int("num_repeats_x", &num_repeats_x);
	register_int("num_repeats_y", &num_repeats_y);
	register_vec2("num_repeats", uniform_num_repeats);
}

string ComplexModulateEffect::output_fragment_shader()
{
	return read_file("complex_modulate_effect.frag");
}

void ComplexModulateEffect::set_gl_state(GLuint glsl_program_num, const string &prefix, unsigned *sampler_num)
{
	Effect::set_gl_state(glsl_program_num, prefix, sampler_num);

	uniform_num_repeats[0] = float(num_repeats_x);
	uniform_num_repeats[1] = float(num_repeats_y);

	// Set the secondary input to repeat (and nearest while we're at it).
	Node *self = chain->find_node_for_effect(this);
	glActiveTexture(chain->get_input_sampler(self, 1));
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	check_error();
}

void ComplexModulateEffect::inform_input_size(unsigned input_num, unsigned width, unsigned height)
{
	if (input_num == 0) {
		primary_input_width = width;
		primary_input_height = height;
	}
}

void ComplexModulateEffect::get_output_size(unsigned *width, unsigned *height,
                                            unsigned *virtual_width, unsigned *virtual_height) const
{
	*width = *virtual_width = primary_input_width;
	*height = *virtual_height = primary_input_height;
}

}  // namespace movit
