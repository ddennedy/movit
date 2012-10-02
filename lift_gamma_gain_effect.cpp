#define GL_GLEXT_PROTOTYPES 1

#include <math.h>
#include <GL/gl.h>
#include <GL/glext.h>

#include "lift_gamma_gain_effect.h"
#include "util.h"

LiftGammaGainEffect::LiftGammaGainEffect()
	: lift(0.0f, 0.0f, 0.0f),
	  gamma(1.0f, 1.0f, 1.0f),
	  gain(1.0f, 1.0f, 1.0f)
{
	register_vec3("lift", (float *)&lift);
	register_vec3("gamma", (float *)&gamma);
	register_vec3("gain", (float *)&gain);
}

std::string LiftGammaGainEffect::output_fragment_shader()
{
	return read_file("lift_gamma_gain_effect.glsl");
}

void LiftGammaGainEffect::set_uniforms(GLuint glsl_program_num, const std::string &prefix)
{
	Effect::set_uniforms(glsl_program_num, prefix);

	RGBTriplet gain_pow_inv_gamma(
		pow(gain.r, 1.0f / gamma.r),
		pow(gain.g, 1.0f / gamma.g),
		pow(gain.b, 1.0f / gamma.b));
	set_uniform_vec3(glsl_program_num, prefix, "gain_pow_inv_gamma", (float *)&gain_pow_inv_gamma);

	RGBTriplet inv_gamma_22(
		2.2f / gamma.r,
		2.2f / gamma.g,
		2.2f / gamma.b);
	set_uniform_vec3(glsl_program_num, prefix, "inv_gamma_22", (float *)&inv_gamma_22);
}
