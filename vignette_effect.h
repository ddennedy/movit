#ifndef _MOVIT_VIGNETTE_EFFECT_H
#define _MOVIT_VIGNETTE_EFFECT_H 1

// A circular vignette, falling off as cos² of the distance from the center
// (the classic formula for approximating a real lens).

#include <epoxy/gl.h>
#include <string>

#include "effect.h"

namespace movit {

class VignetteEffect : public Effect {
public:
	VignetteEffect();
	std::string effect_type_id() const override { return "VignetteEffect"; }
	std::string output_fragment_shader() override;

	bool needs_srgb_primaries() const override { return false; }
	AlphaHandling alpha_handling() const override { return DONT_CARE_ALPHA_TYPE; }
	bool strong_one_to_one_sampling() const override { return true; }

	void inform_input_size(unsigned input_num, unsigned width, unsigned height) override;
	void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num) override;

private:
	Point2D center;
	Point2D uniform_aspect_correction, uniform_flipped_center;
	float radius, inner_radius;
	float uniform_pihalf_div_radius;
};

}  // namespace movit

#endif // !defined(_MOVIT_VIGNETTE_EFFECT_H)
