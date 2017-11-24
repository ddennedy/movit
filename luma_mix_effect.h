#ifndef _MOVIT_LUMA_MIX_EFFECT_H
#define _MOVIT_LUMA_MIX_EFFECT_H 1

// Fade between two images based on a third monochrome one; lighter pixels
// will be faded before darker pixels (unless the inverse flag is set,
// in which case darker pixels will be faded before lighter pixels).
// This allows a wide range of different video wipes implemented using
// a single effect.
//
// Note that despite the name, the third input's _red_ channel is what's used
// for transitions; there is no luma calculation done. If you need that,
// put a SaturationEffect in front to desaturate (which calculates luma).

#include <string>

#include "effect.h"

namespace movit {

class LumaMixEffect : public Effect {
public:
	LumaMixEffect();
	std::string effect_type_id() const override { return "LumaMixEffect"; }
	std::string output_fragment_shader() override;
	void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num) override;

	bool needs_srgb_primaries() const override { return false; }
	unsigned num_inputs() const override { return 3; }
	bool strong_one_to_one_sampling() const override { return true; }
	AlphaHandling alpha_handling() const override { return INPUT_PREMULTIPLIED_ALPHA_KEEP_BLANK; }

private:
	float transition_width, progress;
	int inverse;  // 0 or 1.
	bool uniform_inverse;
	float uniform_progress_mul_w_plus_one;
};

}  // namespace movit

#endif // !defined(_MOVIT_MIX_EFFECT_H)
