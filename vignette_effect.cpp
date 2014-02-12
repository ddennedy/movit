#include <GL/glew.h>
#include <assert.h>
#include <math.h>

#include "effect_util.h"
#include "util.h"
#include "vignette_effect.h"

using namespace std;

namespace movit {

VignetteEffect::VignetteEffect()
	: center(0.5f, 0.5f),
	  aspect_correction(1.0f, 1.0f),
	  radius(0.3f),
	  inner_radius(0.3f)
{
	register_vec2("center", (float *)&center);
	register_float("radius", (float *)&radius);
	register_float("inner_radius", (float *)&inner_radius);
}

string VignetteEffect::output_fragment_shader()
{
	return read_file("vignette_effect.frag");
}

void VignetteEffect::inform_input_size(unsigned input_num, unsigned width, unsigned height) {
	assert(input_num == 0);
	if (width >= height) {
		aspect_correction = Point2D(float(width) / float(height), 1.0f);
	} else {
		aspect_correction = Point2D(1.0f, float(height) / float(width));
	}
}

void VignetteEffect::set_gl_state(GLuint glsl_program_num, const string &prefix, unsigned *sampler_num)
{
	Effect::set_gl_state(glsl_program_num, prefix, sampler_num);

	set_uniform_float(glsl_program_num, prefix, "pihalf_div_radius", 0.5 * M_PI / radius);
	set_uniform_vec2(glsl_program_num, prefix, "aspect_correction", (float *)&aspect_correction);

	Point2D flipped_center(center.x, 1.0f - center.y);
	set_uniform_vec2(glsl_program_num, prefix, "flipped_center", (float *)&flipped_center);
}

}  // namespace movit
