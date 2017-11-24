#ifndef _MOVIT_WHITE_BALANCE_EFFECT_H
#define _MOVIT_WHITE_BALANCE_EFFECT_H 1

// Color correction in LMS color space.

#include <epoxy/gl.h>
#include <string>
#include <Eigen/Core>

#include "effect.h"

namespace movit {

class WhiteBalanceEffect : public Effect {
public:
	WhiteBalanceEffect();
	std::string effect_type_id() const override { return "WhiteBalanceEffect"; }
	AlphaHandling alpha_handling() const override { return DONT_CARE_ALPHA_TYPE; }
	bool strong_one_to_one_sampling() const override { return true; }
	std::string output_fragment_shader() override;

	void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num) override;

private:
	// The neutral color, in linear sRGB.
	RGBTriplet neutral_color;

	// Output color temperature (in Kelvins).
	// Choosing 6500 will lead to no color cast (ie., the neutral color becomes perfectly gray).
	float output_color_temperature;

	Eigen::Matrix3d uniform_correction_matrix;
};

}  // namespace movit

#endif // !defined(_MOVIT_WHITE_BALANCE_EFFECT_H)
