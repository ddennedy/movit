#include <GL/glew.h>
#include <math.h>

#include "effect_util.h"
#include "fft_pass_effect.h"
#include "util.h"

using namespace std;

FFTPassEffect::FFTPassEffect()
	: input_width(1280),
	  input_height(720),
	  direction(HORIZONTAL)
{
	register_int("fft_size", &fft_size);
	register_int("direction", (int *)&direction);
	register_int("pass_number", &pass_number);
	register_int("inverse", &inverse);
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

	int input_size = (direction == VERTICAL) ? input_height : input_width;

	// See the comments on changes_output_size() in the .h file to see
	// why this is legal. It is _needed_ because it counteracts the
	// precision issues we get because we sample the input texture with
	// normalized coordinates (especially when the repeat count along
	// the axis is not a power of two); we very rapidly end up in narrowly
	// missing a texel center, which causes precision loss to propagate
	// throughout the FFT.
	assert(*sampler_num == 1);
	glActiveTexture(GL_TEXTURE0);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	check_error();

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
	float *tmp = new float[fft_size * 4];
	int subfft_size = 1 << pass_number;
	double mulfac;
	if (inverse) {
		mulfac = 2.0 * M_PI;
	} else {
		mulfac = -2.0 * M_PI;
	}

	assert((fft_size & (fft_size - 1)) == 0);  // Must be power of two.
	assert(fft_size % subfft_size == 0);
	int stride = fft_size / subfft_size;
	for (int i = 0; i < fft_size; ++i) {
		int k = i / stride;         // Element number within this sub-FFT.
		int offset = i % stride;    // Sub-FFT number.
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
		int base = k * stride * 2 + offset;
		int support_texture_index;
		if (direction == FFTPassEffect::VERTICAL) {
			// Compensate for OpenGL's bottom-left convention.
			support_texture_index = fft_size - i - 1;
		} else {
			support_texture_index = i;
		}
		tmp[support_texture_index * 4 + 0] = (base - support_texture_index) / double(input_size);
		tmp[support_texture_index * 4 + 1] = (base + stride - support_texture_index) / double(input_size);
		tmp[support_texture_index * 4 + 2] = twiddle_real;
		tmp[support_texture_index * 4 + 3] = twiddle_imag;
	}

	glActiveTexture(GL_TEXTURE0 + *sampler_num);
	check_error();
	glBindTexture(GL_TEXTURE_1D, tex);
	check_error();
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	check_error();
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	check_error();
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	check_error();

	// Supposedly FFTs are very sensitive to inaccuracies in the twiddle factors,
	// at least according to a paper by Schatzman (see gpuwave.pdf reference [30]
	// for the full reference), so we keep them at 32-bit. However, for
	// small sizes, all components are exact anyway, so we can cheat there
	// (although noting that the source coordinates become somewhat less
	// accurate then, too).
	glTexImage1D(GL_TEXTURE_1D, 0, (subfft_size <= 4) ? GL_RGBA16F : GL_RGBA32F, fft_size, 0, GL_RGBA, GL_FLOAT, tmp);
	check_error();

	delete[] tmp;

	set_uniform_int(glsl_program_num, prefix, "support_tex", *sampler_num);
	++*sampler_num;

	assert(input_size % fft_size == 0);
	set_uniform_float(glsl_program_num, prefix, "num_repeats", input_size / fft_size);
}
