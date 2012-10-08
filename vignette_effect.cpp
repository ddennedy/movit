#include <math.h>

#include "vignette_effect.h"
#include "util.h"
#include "opengl.h"

VignetteEffect::VignetteEffect()
	: center(0.5f, 0.5f),
	  radius(0.3f),
	  inner_radius(0.3f)
{
	register_vec2("center", (float *)&center);
	register_float("radius", (float *)&radius);
	register_float("inner_radius", (float *)&inner_radius);
}

std::string VignetteEffect::output_fragment_shader()
{
	return read_file("vignette_effect.frag");
}

void VignetteEffect::set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num)
{
	Effect::set_gl_state(glsl_program_num, prefix, sampler_num);

	set_uniform_float(glsl_program_num, prefix, "pihalf_div_radius", 0.5 * M_PI / radius);

	Point2D aspect(16.0f / 9.0f, 1.0f);  // FIXME
	set_uniform_vec2(glsl_program_num, prefix, "aspect_correction", (float *)&aspect);
}
