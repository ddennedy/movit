#define GL_GLEXT_PROTOTYPES 1

#include <math.h>
#include <GL/gl.h>
#include <GL/glext.h>

#include "blur_effect.h"
#include "util.h"

BlurEffect::BlurEffect()
	: radius(0.3f)
{
	register_float("radius", (float *)&radius);
}

std::string BlurEffect::output_fragment_shader()
{
	return read_file("blur_effect.frag");
}

void BlurEffect::set_uniforms(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num)
{
	Effect::set_uniforms(glsl_program_num, prefix, sampler_num);
}
