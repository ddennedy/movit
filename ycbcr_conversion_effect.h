#ifndef _MOVIT_YCBCR_CONVERSION_EFFECT_H
#define _MOVIT_YCBCR_CONVERSION_EFFECT_H 1

// Converts from R'G'B' to Y'CbCr; that is, more or less the opposite of YCbCrInput,
// except that it keeps the data as 4:4:4 chunked Y'CbCr; you'll need to subsample
// and/or convert to planar somehow else.

#include <epoxy/gl.h>
#include <Eigen/Core>
#include <string>

#include "effect.h"
#include "ycbcr.h"

namespace movit {

class YCbCrConversionEffect : public Effect {
private:
	// Should not be instantiated by end users;
	// call EffectChain::add_ycbcr_output() instead.
	YCbCrConversionEffect(const YCbCrFormat &ycbcr_format, GLenum type);
	friend class EffectChain;

public:
	std::string effect_type_id() const override { return "YCbCrConversionEffect"; }
	std::string output_fragment_shader() override;
	void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num) override;
	AlphaHandling alpha_handling() const override { return DONT_CARE_ALPHA_TYPE; }
	bool strong_one_to_one_sampling() const override { return true; }

	// Should not be called by end users; call
	// EffectChain::change_ycbcr_output_format() instead.
	void change_output_format(const YCbCrFormat &ycbcr_format) {
		this->ycbcr_format = ycbcr_format;
	}

private:
	YCbCrFormat ycbcr_format;
	GLenum type;

	Eigen::Matrix3d uniform_ycbcr_matrix;
	float uniform_offset[3];
	bool uniform_clamp_range;
	float uniform_ycbcr_min[3], uniform_ycbcr_max[3];
};

}  // namespace movit

#endif // !defined(_MOVIT_YCBCR_CONVERSION_EFFECT_H)
