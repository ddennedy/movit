#ifndef _MOVIT_DITHER_EFFECT_H
#define _MOVIT_DITHER_EFFECT_H 1

// Implements simple rectangular-PDF dither.
//
// Although all of our processing internally is in floating-point (a mix of 16-
// and 32-bit), eventually most pipelines will end up downconverting to a fixed-point
// format, typically 8-bits unsigned integer (GL_RGBA8).
//
// The hardware will typically do proper rounding for us, so that we minimize
// quantization noise, but for some applications, if you look closely, you can still
// see some banding; 8 bits is not really all that much (and if we didn't have the
// perceptual gamma curve, it would be a lot worse).
//
// The standard solution to this is dithering; in short, to add a small random component
// to each pixel before quantization. This increases the overall noise floor slightly,
// but allows us to represent frequency components with an amplitude lower than 1/256.
// 
// My standard reference on dither is:
//
//   Cameron Nicklaus Christou: “Optimal Dither and Noise Shaping in Image Processing”
//   http://uwspace.uwaterloo.ca/bitstream/10012/3867/1/thesis.pdf
//
// However, we need to make two significant deviations from the recommendations it makes.
// First of all, it recommends using a triangular-PDF (TPDF) dither (which can be synthesized
// effectively by adding two uniformly distributed random numbers) instead of rectangular-PDF
// (RPDF; using one uniformly distributed random number), in order to make the second moment
// of the error signal independent from the original image. However, since the recommended
// TPDF must be twice as wide as the RPDF, it means it can go to +/- 1, which means that
// some of the time, it will add enough noise to change a pixel just by itself. Given that
// a very common use case for us is converting 8-bit -> 8-bit (ie., no bit reduction at all),
// it would seem like a more important goal to have no noise in that situation than to
// improve the dither further.
//
// Second, the thesis recommends noise shaping (also known as error diffusion in the image
// processing world). This is, however, very hard to implement properly on a GPU, since it
// almost by definition feeds the value of output pixels into the neighboring input pixels.
// Maybe one could make a version that implemented the noise shapers by way of FIR filters
// instead of IIR like this, but it would seem a lot of work for very subtle gain.
//
// We keep the dither noise fixed as long as the output resolution doesn't change;
// this ensures we don't upset video codecs too much. (One could also dither in time,
// like many LCD monitors do, but it starts to get very hairy, again, for limited gains.)
// The dither is also deterministic across runs.

#include <epoxy/gl.h>
#include <string>

#include "effect.h"

namespace movit {

class DitherEffect : public Effect {
private:
	// Should not be instantiated by end users;
	// call EffectChain::set_dither_bits() instead.
	DitherEffect();
	friend class EffectChain;

public:
	~DitherEffect();
	std::string effect_type_id() const override { return "DitherEffect"; }
	std::string output_fragment_shader() override;

	// Note that if we did error diffusion, we'd actually want to diffuse the
	// premultiplied error. However, we need to do dithering in the same
	// space as quantization, whether that be pre- or postmultiply.
	AlphaHandling alpha_handling() const override { return DONT_CARE_ALPHA_TYPE; }
	bool strong_one_to_one_sampling() const override { return true; }

	void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num) override;

private:
	void update_texture(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num);

	int width, height, num_bits;
	int last_width, last_height, last_num_bits;
	int texture_width, texture_height;

	GLuint texnum;
	float uniform_round_fac, uniform_inv_round_fac;
	float uniform_tc_scale[2];
	GLint uniform_dither_tex;
};

}  // namespace movit

#endif // !defined(_MOVIT_DITHER_EFFECT_H)
