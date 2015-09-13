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
	virtual std::string effect_type_id() const { return "WhiteBalanceEffect"; }
	virtual AlphaHandling alpha_handling() const { return DONT_CARE_ALPHA_TYPE; }
	virtual bool one_to_one_sampling() const { return true; }
	std::string output_fragment_shader();

	void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num);

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
