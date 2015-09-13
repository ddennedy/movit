#include <epoxy/gl.h>
#include <math.h>

#include "effect_util.h"
#include "lift_gamma_gain_effect.h"
#include "util.h"

using namespace std;

namespace movit {

LiftGammaGainEffect::LiftGammaGainEffect()
	: lift(0.0f, 0.0f, 0.0f),
	  gamma(1.0f, 1.0f, 1.0f),
	  gain(1.0f, 1.0f, 1.0f)
{
	register_vec3("lift", (float *)&lift);
	register_vec3("gamma", (float *)&gamma);
	register_vec3("gain", (float *)&gain);
	register_uniform_vec3("gain_pow_inv_gamma", (float *)&uniform_gain_pow_inv_gamma);
	register_uniform_vec3("inv_gamma_22", (float *)&uniform_inv_gamma22);
}

string LiftGammaGainEffect::output_fragment_shader()
{
	return read_file("lift_gamma_gain_effect.frag");
}

void LiftGammaGainEffect::set_gl_state(GLuint glsl_program_num, const string &prefix, unsigned *sampler_num)
{
	Effect::set_gl_state(glsl_program_num, prefix, sampler_num);

	uniform_gain_pow_inv_gamma = RGBTriplet(
		pow(gain.r, 1.0f / gamma.r),
		pow(gain.g, 1.0f / gamma.g),
		pow(gain.b, 1.0f / gamma.b));

	uniform_inv_gamma22 = RGBTriplet(
		2.2f / gamma.r,
		2.2f / gamma.g,
		2.2f / gamma.b);
}

}  // namespace movit
