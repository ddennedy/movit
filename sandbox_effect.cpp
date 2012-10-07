#include <math.h>
#include <assert.h>

#include "sandbox_effect.h"
#include "util.h"
#include "opengl.h"

SandboxEffect::SandboxEffect()
	: parm(0.0f)
{
	register_float("parm", &parm);
}

std::string SandboxEffect::output_fragment_shader()
{
	return read_file("sandbox_effect.frag");
}

void SandboxEffect::set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num)
{
	Effect::set_gl_state(glsl_program_num, prefix, sampler_num);

	// Any OpenGL state you might want to set, goes here.
}
