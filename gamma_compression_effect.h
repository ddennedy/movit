#ifndef _MOVIT_GAMMA_COMPRESSION_EFFECT_H 
#define _MOVIT_GAMMA_COMPRESSION_EFFECT_H 1

// An effect to convert linear light to the given gamma curve,
// typically inserted by the framework automatically at the end
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

class GammaCompressionEffect : public Effect {
private:
	// Should not be instantiated by end users.
	GammaCompressionEffect();
	friend class EffectChain;

public:
	std::string effect_type_id() const override { return "GammaCompressionEffect"; }
	std::string output_fragment_shader() override;
	void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num) override;

	bool needs_srgb_primaries() const override { return false; }
	bool strong_one_to_one_sampling() const override { return true; }

	// Actually needs postmultiplied input as well as outputting it.
	// EffectChain will take care of that.
	AlphaHandling alpha_handling() const override { return OUTPUT_POSTMULTIPLIED_ALPHA; }

private:
	GammaCurve destination_curve;
	float uniform_linear_scale, uniform_c[5], uniform_beta;
};

}  // namespace movit

#endif // !defined(_MOVIT_GAMMA_COMPRESSION_EFFECT_H)
