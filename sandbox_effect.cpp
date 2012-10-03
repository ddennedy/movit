#define GL_GLEXT_PROTOTYPES 1

#include <math.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <assert.h>

#include "sandbox_effect.h"
#include "util.h"

SandboxEffect::SandboxEffect()
	: parm(0.0f)
{
	register_float("parm", &parm);
}

std::string SandboxEffect::output_fragment_shader()
{
	return read_file("sandbox_effect.frag");
}

void SandboxEffect::set_uniforms(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num)
{
	Effect::set_uniforms(glsl_program_num, prefix, sampler_num);

	// Any OpenGL state you might want to set, goes here.
}
