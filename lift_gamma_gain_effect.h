#ifndef _MOVIT_LIFT_GAMMA_GAIN_EFFECT_H
#define _MOVIT_LIFT_GAMMA_GAIN_EFFECT_H 1

// A simple lift/gamma/gain effect, used for color grading.
//
// Very roughly speaking, lift=shadows, gamma=midtones and gain=highlights,
// although all parameters affect the entire curve. Mathematically speaking,
// it is a bit unusual to look at gamma as a color, but it works pretty well
// in practice.
//
// The classic formula is: output = (gain * (x + lift * (1-x)))^(1/gamma).
//
// The lift is actually a case where we actually would _not_ want linear light;
// since black by definition becomes equal to the lift color, we want lift to
// be pretty close to black, but in linear light that means lift affects the
// rest of the curve relatively little. Thus, we actually convert to gamma 2.2
// before lift, and then back again afterwards. (Gain and gamma are,
// up to constants, commutative with the de-gamma operation.)
//
// Also, gamma is a case where we would not want premultiplied alpha.
// Thus, we have to divide away alpha first, and then re-multiply it back later.

#include <epoxy/gl.h>
#include <string>

#include "effect.h"

namespace movit {

class LiftGammaGainEffect : public Effect {
public:
	LiftGammaGainEffect();
	std::string effect_type_id() const override { return "LiftGammaGainEffect"; }
	AlphaHandling alpha_handling() const override { return INPUT_PREMULTIPLIED_ALPHA_KEEP_BLANK; }
	bool strong_one_to_one_sampling() const override { return true; }
	std::string output_fragment_shader() override;

	void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num) override;

private:
	RGBTriplet lift, gamma, gain;
	RGBTriplet uniform_gain_pow_inv_gamma, uniform_inv_gamma22;
};

}  // namespace movit

#endif // !defined(_MOVIT_LIFT_GAMMA_GAIN_EFFECT_H)
