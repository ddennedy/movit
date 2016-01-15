#include <epoxy/gl.h>
#include <math.h>

#include "effect_chain.h"
#include "effect_util.h"
#include "fp16.h"
#include "fft_pass_effect.h"
#include "util.h"

using namespace std;

namespace movit {

FFTPassEffect::FFTPassEffect()
	: input_width(1280),
	  input_height(720),
	  direction(HORIZONTAL),
	  last_fft_size(-1),
	  last_direction(INVALID),
	  last_pass_number(-1),
	  last_inverse(-1),
	  last_input_size(-1)
{
	register_int("fft_size", &fft_size);
	register_int("direction", (int *)&direction);
	register_int("pass_number", &pass_number);
	register_int("inverse", &inverse);
	register_uniform_float("num_repeats", &uniform_num_repeats);
	register_uniform_sampler2d("support_tex", &uniform_support_tex);
	glGenTextures(1, &tex);
}

FFTPassEffect::~FFTPassEffect()
{
	glDeleteTextures(1, &tex);
}

string FFTPassEffect::output_fragment_shader()
{
	char buf[256];
	sprintf(buf, "#define DIRECTION_VERTICAL %d\n", (direction == VERTICAL));
	return buf + read_file("fft_pass_effect.frag");
}

void FFTPassEffect::set_gl_state(GLuint glsl_program_num, const string &prefix, unsigned *sampler_num)
{
	Effect::set_gl_state(glsl_program_num, prefix, sampler_num);

	// This is needed because it counteracts the precision issues we get
	// because we sample the input texture with normalized coordinates
	// (especially when the repeat count along the axis is not a power of
	// two); we very rapidly end up in narrowly missing a texel center,
	// which causes precision loss to propagate throughout the FFT.
	Node *self = chain->find_node_for_effect(this);
	glActiveTexture(chain->get_input_sampler(self, 0));
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	check_error();

	// Because of the memory layout (see below) and because we use offsets,
	// the support texture values for many consecutive values will be
	// the same. Thus, we can store a smaller texture (giving a small
	// performance boost) and just sample it with NEAREST. Also, this
	// counteracts any precision issues we might get from linear
	// interpolation.
	glActiveTexture(GL_TEXTURE0 + *sampler_num);
	check_error();
	glBindTexture(GL_TEXTURE_2D, tex);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	check_error();

	int input_size = (direction == VERTICAL) ? input_height : input_width;
	if (last_fft_size != fft_size ||
	    last_direction != direction ||
	    last_pass_number != pass_number ||
	    last_inverse != inverse ||
	    last_input_size != input_size) {
		generate_support_texture();
	}

	uniform_support_tex = *sampler_num;
	++*sampler_num;

	assert(input_size % fft_size == 0);
	uniform_num_repeats = input_size / fft_size;
}

void FFTPassEffect::generate_support_texture()
{
	int input_size = (direction == VERTICAL) ? input_height : input_width;

	// The memory layout follows figure 5.2 on page 25 of
	// http://gpuwave.sesse.net/gpuwave.pdf -- it can be a bit confusing
	// at first, but is classically explained more or less as follows:
	//
	// The classic Cooley-Tukey decimation-in-time FFT algorithm works
	// by first splitting input data into odd and even elements
	// (e.g. bit-wise xxxxx0 and xxxxx1 for a size-32 FFT), then FFTing
	// them separately and combining them using twiddle factors.
	// So the outer pass (done _last_) looks only at the last bit,
	// and does one such merge pass of sub-size N/2 (FFT size N).
	//
	// FFT of the first part must then necessarily be split into xxxx00 and
	// xxxx10, and similarly xxxx01 and xxxx11 for the other part. Since
	// these two FFTs are handled identically, it means we split into xxxx0x
	// and xxxx1x, so that the second-outer pass (done second-to-last)
	// looks only at the second last bit, and so on. We do two such merge
	// passes of sub-size N/4 (sub-FFT size N/2).
	//
	// Thus, the inner, Nth pass (done first) splits at the first bit,
	// so 0 is paired with 16, 1 with 17 and so on, doing N/2 such merge
	// passes of sub-size 1 (sub-FFT size 2). We say that the stride is 16.
	// The second-inner, (N-1)th pass (done second) splits at the second
	// bit, so the stride is 8, and so on.

	assert((fft_size & (fft_size - 1)) == 0);  // Must be power of two.
	int subfft_size = 1 << pass_number;
	fp16_int_t *tmp = new fp16_int_t[subfft_size * 4];
	double mulfac;
	if (inverse) {
		mulfac = 2.0 * M_PI;
	} else {
		mulfac = -2.0 * M_PI;
	}

	assert((fft_size & (fft_size - 1)) == 0);  // Must be power of two.
	assert(fft_size % subfft_size == 0);
	int stride = fft_size / subfft_size;
	for (int i = 0; i < subfft_size; i++) {
		int k = i;
		double twiddle_real, twiddle_imag;

		if (k < subfft_size / 2) {
			twiddle_real = cos(mulfac * (k / double(subfft_size)));
			twiddle_imag = sin(mulfac * (k / double(subfft_size)));
		} else {
			// This is mathematically equivalent to the twiddle factor calculations
			// in the other branch of the if, but not numerically; the range
			// reductions on x87 are not all that precise, and this keeps us within
			// [0,pi>.
			k -= subfft_size / 2;
			twiddle_real = -cos(mulfac * (k / double(subfft_size)));
			twiddle_imag = -sin(mulfac * (k / double(subfft_size)));
		}

		// The support texture contains everything we need for the FFT:
		// Obviously, the twiddle factor (in the Z and W components), but also
		// which two samples to fetch. These are stored as normalized
		// X coordinate offsets (Y coordinate for a vertical FFT); the reason
		// for using offsets and not direct coordinates as in GPUwave
		// is that we can have multiple FFTs along the same line,
		// and want to reuse the support texture by repeating it.
		int base = k * stride * 2;
		int support_texture_index = i;
		int src1 = base;
		int src2 = base + stride;
		double sign = 1.0;
		if (direction == FFTPassEffect::VERTICAL) {
			// Compensate for OpenGL's bottom-left convention.
			support_texture_index = subfft_size - support_texture_index - 1;
			sign = -1.0;
		}
		tmp[support_texture_index * 4 + 0] = fp32_to_fp16(sign * (src1 - i * stride) / double(input_size));
		tmp[support_texture_index * 4 + 1] = fp32_to_fp16(sign * (src2 - i * stride) / double(input_size));
		tmp[support_texture_index * 4 + 2] = fp32_to_fp16(twiddle_real);
		tmp[support_texture_index * 4 + 3] = fp32_to_fp16(twiddle_imag);
	}

	// Supposedly FFTs are very sensitive to inaccuracies in the twiddle factors,
	// at least according to a paper by Schatzman (see gpuwave.pdf reference [30]
	// for the full reference); however, practical testing indicates that it's
	// not a problem to keep the twiddle factors at 16-bit, at least as long as
	// we round them properly--it would seem that Schatzman were mainly talking
	// about poor sin()/cos() approximations. Thus, we store them in 16-bit,
	// which gives a nice speed boost.
	//
	// Note that the source coordinates become somewhat less accurate too, though.
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, subfft_size, 1, 0, GL_RGBA, GL_HALF_FLOAT, tmp);
	check_error();

	delete[] tmp;

	last_fft_size = fft_size;
	last_direction = direction;
	last_pass_number = pass_number;
	last_inverse = inverse;
	last_input_size = input_size;
}

}  // namespace movit
