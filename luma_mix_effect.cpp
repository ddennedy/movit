#include "luma_mix_effect.h"
#include "effect_util.h"
#include "util.h"

using namespace std;

namespace movit {

LumaMixEffect::LumaMixEffect()
	: transition_width(1.0f), progress(0.5f), inverse(0)
{
	register_float("transition_width", &transition_width);
	register_float("progress", &progress);
	register_int("inverse", &inverse);
	register_uniform_bool("bool_inverse", &uniform_inverse);
	register_uniform_float("progress_mul_w_plus_one", &uniform_progress_mul_w_plus_one);
}

string LumaMixEffect::output_fragment_shader()
{
	return read_file("luma_mix_effect.frag");
}

void LumaMixEffect::set_gl_state(GLuint glsl_program_num, const string &prefix, unsigned *sampler_num)
{
	Effect::set_gl_state(glsl_program_num, prefix, sampler_num);
	uniform_progress_mul_w_plus_one = progress * (transition_width + 1.0);
	uniform_inverse = inverse;
}

}  // namespace movit
