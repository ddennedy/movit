#ifndef _MOVIT_COMPLEX_MODULATE_EFFECT_H
#define _MOVIT_COMPLEX_MODULATE_EFFECT_H 1

// An effect that treats each pixel as two complex numbers (xy and zw),
// and multiplies it with some other complex number (xy and xy, so the
// same in both cases). The latter can be repeated both horizontally and
// vertically if desired.
//
// The typical use is to implement convolution by way of FFT; since
// FFT(A ⊙ B) = FFT(A) * FFT(B), you can FFT both inputs (where B
// would often even be a constant, so you'd only need to do FFT once),
// multiply them together and then IFFT the result to get a convolution.
//
// It is in a sense “wrong” to do this directly on pixels, since the color
// channels are independent and real-valued (ie., not complex numbers), but
// since convolution is a linear operation, it's unproblematic to treat R + Gi
// as a single complex number and B + Ai and another one; barring numerical
// errors, there should be no leakage between the channels as long as you're
// convolving with a real quantity. (There are more sophisticated ways of doing
// two real FFTs with a single complex one, but we won't need them, as we
// don't care about the actual FFT result, just that the convolution property
// holds.)

#include <epoxy/gl.h>
#include <string>

#include "effect.h"

namespace movit {

class EffectChain;

class ComplexModulateEffect : public Effect {
public:
	ComplexModulateEffect();
	std::string effect_type_id() const override { return "ComplexModulateEffect"; }
	std::string output_fragment_shader() override;

	// Technically we only need texture bounce for the second input
	// (to be allowed to mess with its sampler state), but there's
	// no way of expressing that currently.
	bool needs_texture_bounce() const override { return true; }
	bool changes_output_size() const override { return true; }
	bool sets_virtual_output_size() const override { return false; }

	void inform_input_size(unsigned input_num, unsigned width, unsigned height) override;
	void get_output_size(unsigned *width, unsigned *height,
	                     unsigned *virtual_width, unsigned *virtual_height) const override;
	unsigned num_inputs() const override { return 2; }
	void inform_added(EffectChain *chain) override { this->chain = chain; }

	void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num) override;

private:
	EffectChain *chain;
	int primary_input_width, primary_input_height;
	int num_repeats_x, num_repeats_y;
	float uniform_num_repeats[2];
};

}  // namespace movit

#endif // !defined(_MOVIT_COMPLEX_MODULATE_EFFECT_H)
