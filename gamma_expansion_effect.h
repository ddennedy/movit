#ifndef _MOVIT_GAMMA_EXPANSION_EFFECT_H 
#define _MOVIT_GAMMA_EXPANSION_EFFECT_H 1

// An effect to convert the given gamma curve into linear light,
// typically inserted by the framework automatically at the beginning
// of the processing chain.
//
// Currently supports sRGB, Rec. 601/709 and Rec. 2020 (10- and 12-bit).
// Note that Movit's internal formats generally do not have enough accuracy
// for 12-bit input or output.

#include <epoxy/gl.h>
#include <string>

#include "effect.h"
#include "image_format.h"

namespace movit {

class GammaExpansionEffect : public Effect {
private:
	// Should not be instantiated by end users.
	GammaExpansionEffect();
	friend class EffectChain;

public:
	virtual std::string effect_type_id() const { return "GammaExpansionEffect"; }
	std::string output_fragment_shader();
	virtual void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num);

	virtual bool needs_linear_light() const { return false; }
	virtual bool needs_srgb_primaries() const { return false; }
	virtual bool one_to_one_sampling() const { return true; }

	// Actually processes its input in a nonlinear fashion,
	// but does not touch alpha, and we are a special case anyway.
	virtual AlphaHandling alpha_handling() const { return DONT_CARE_ALPHA_TYPE; }

private:
	GammaCurve source_curve;
	float uniform_linear_scale, uniform_c[5], uniform_beta;
};

}  // namespace movit

#endif // !defined(_MOVIT_GAMMA_EXPANSION_EFFECT_H)
