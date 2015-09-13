#include <epoxy/gl.h>
#include <assert.h>
#include <math.h>

#include "effect_util.h"
#include "util.h"
#include "vignette_effect.h"

using namespace std;

namespace movit {

VignetteEffect::VignetteEffect()
	: center(0.5f, 0.5f),
	  uniform_aspect_correction(1.0f, 1.0f),
	  uniform_flipped_center(0.5f, 0.5f),
	  radius(0.3f),
	  inner_radius(0.3f)
{
	register_vec2("center", (float *)&center);
	register_float("radius", (float *)&radius);
	register_float("inner_radius", (float *)&inner_radius);
	register_uniform_float("pihalf_div_radius", &uniform_pihalf_div_radius);
	register_uniform_vec2("aspect_correction", (float *)&uniform_aspect_correction);
	register_uniform_vec2("flipped_center", (float *)&uniform_flipped_center);
}

string VignetteEffect::output_fragment_shader()
{
	return read_file("vignette_effect.frag");
}

void VignetteEffect::inform_input_size(unsigned input_num, unsigned width, unsigned height) {
	assert(input_num == 0);
	if (width >= height) {
		uniform_aspect_correction = Point2D(float(width) / float(height), 1.0f);
	} else {
		uniform_aspect_correction = Point2D(1.0f, float(height) / float(width));
	}
}

void VignetteEffect::set_gl_state(GLuint glsl_program_num, const string &prefix, unsigned *sampler_num)
{
	Effect::set_gl_state(glsl_program_num, prefix, sampler_num);

	uniform_pihalf_div_radius = 0.5 * M_PI / radius;
	uniform_flipped_center = Point2D(center.x, 1.0f - center.y);
}

}  // namespace movit
