#ifndef _MOVIT_FFT_PASS_EFFECT_H
#define _MOVIT_FFT_PASS_EFFECT_H 1

// One pass of a radix-2, in-order, decimation-in-time 1D FFT/IFFT. If you
// connect multiple ones of these together, you will eventually have a complete
// FFT or IFFT. The FFTed data is not so useful for video effects in itself,
// but enables faster convolutions (especially non-separable 2D convolutions)
// than can be done directly, by doing FFT -> multiply -> IFFT. The utilities
// for doing this efficiently will probably be added to Movit at a later date;
// for now, this effect isn't the most useful.
//
// An introduction to FFTs is outside the scope of a file-level comment; see
// http://en.wikipedia.org/wiki/Cooley%E2%80%93Tukey_FFT_algorithm#The_radix-2_DIT_case .
//
// The pixels are not really interpreted as pixels, but are interpreted as two
// complex numbers with (real,imaginary) parts stored in (R,G) and (B,A).
// On top of this two-way parallelism, many FFTs are done in parallel (see below).
//
// Implementing a high-performance FFT on the GPU is not easy, especially
// within the demands of Movit filters. (This is one of the places where
// using CUDA or D3D would be easier, as both ship with pre-made and highly
// tuned FFTs.) We don't go to great lengths to get an optimal implementation,
// but rather stay with someting simple. I'll conveniently enough refer to
// my own report on this topic from 2007, namely
//
//    Steinar H. Gunderson: “GPUwave: An implementation of the split-step
//    Fourier method for the GPU”, http://gpuwave.sesse.net/gpuwave.pdf
//
// Chapter 5 contains the details of the FFT. We follow this rather closely,
// with the exception that in Movit, we only ever draw a single quad,
// so the strategy used in GPUwave with drawing multiple quads with constant
// twiddle factors on them will not be in use here. (It requires some
// benchmarking to find the optimal crossover point anyway.)
//
// Also, we support doing many FFTs along the same axis, so e.g. if you
// have a 128x128 image and ask for a horizontal FFT of size 64, you will
// actually get 256 of them (two wide, 128 high). This is in contrast with
// GPUwave, which only supports them one wide; in a picture setting,
// moving blocks around to create only one block wide FFTs would rapidly
// lead to way too slender textures to be practical (e.g., 1280x720
// with an FFT of size 64 would be 64x14400 rearranged, and many GPUs
// have limits of 8192 pixels or even 2048 along one dimension).
//
// Note that this effect produces an _unnormalized_ FFT, which means that a
// FFT -> IFFT chain will end up not returning the original data (even modulo
// precision errors) but rather the original data with each element multiplied
// by N, the FFT size. As the FFT and IFFT contribute equally to this energy
// gain, it is recommended that you do the division by N after the FFT but
// before the IFFT. This way, you use the least range possible (for one
// scaling), and as fp16 has quite limited range at times, this can be relevant
// on some GPUs for larger sizes.

#include <epoxy/gl.h>
#include <assert.h>
#include <stdio.h>
#include <string>

#include "effect.h"

namespace movit {

class FFTPassEffect : public Effect {
public:
	FFTPassEffect();
	~FFTPassEffect();
	std::string effect_type_id() const override {
		char buf[256];
		if (inverse) {
			snprintf(buf, sizeof(buf), "IFFTPassEffect[%d]", (1 << pass_number));
		} else {
			snprintf(buf, sizeof(buf), "FFTPassEffect[%d]", (1 << pass_number));
		}
		return buf;
	}
	std::string output_fragment_shader() override;

	void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num) override;

	// We don't actually change the output size, but this flag makes sure
	// that no other effect is chained after us. This is important since
	// we cannot deliver filtered results; any attempt at sampling in-between
	// pixels would necessarily give garbage. In addition, we set our sampling
	// mode to GL_NEAREST, which other effects are not ready for; so, the
	// combination of these two flags guarantee that we're run entirely alone
	// in our own phase, which is exactly what we want.
	bool needs_texture_bounce() const override { return true; }
	bool changes_output_size() const override { return true; }
	bool sets_virtual_output_size() const override { return false; }

	void inform_input_size(unsigned input_num, unsigned width, unsigned height) override
	{
		assert(input_num == 0);
		input_width = width;
		input_height = height;
	}
	
	void get_output_size(unsigned *width, unsigned *height,
	                     unsigned *virtual_width, unsigned *virtual_height) const override {
		*width = *virtual_width = input_width;
		*height = *virtual_height = input_height;
	}

	void inform_added(EffectChain *chain) override { this->chain = chain; }
	
	enum Direction { INVALID = -1, HORIZONTAL = 0, VERTICAL = 1 };

private:
	void generate_support_texture();

	EffectChain *chain;
	int input_width, input_height;
	GLuint tex;
	float uniform_num_repeats;
	GLint uniform_support_tex;

	int fft_size;
	Direction direction;
	int pass_number;  // From 1..n.
	int inverse;  // 0 = forward (FFT), 1 = reverse (IFFT).

	int last_fft_size;
	Direction last_direction;
	int last_pass_number;
	int last_inverse;
	int last_input_size;
};

}  // namespace movit

#endif // !defined(_MOVIT_FFT_PASS_EFFECT_H)
