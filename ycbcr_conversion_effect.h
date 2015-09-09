#ifndef _MOVIT_YCBCR_CONVERSION_EFFECT_H
#define _MOVIT_YCBCR_CONVERSION_EFFECT_H 1

// Converts from R'G'B' to Y'CbCr; that is, more or less the opposite of YCbCrInput,
// except that it keeps the data as 4:4:4 chunked Y'CbCr; you'll need to subsample
// and/or convert to planar somehow else.

#include <epoxy/gl.h>
#include <string>

#include "effect.h"
#include "ycbcr.h"

namespace movit {

class YCbCrConversionEffect : public Effect {
private:
	// Should not be instantiated by end users;
	// call EffectChain::add_ycbcr_output() instead.
	YCbCrConversionEffect(const YCbCrFormat &ycbcr_format);
	friend class EffectChain;

public:
	virtual std::string effect_type_id() const { return "YCbCrConversionEffect"; }
	std::string output_fragment_shader();
	virtual AlphaHandling alpha_handling() const { return DONT_CARE_ALPHA_TYPE; }
	virtual bool one_to_one_sampling() const { return true; }

private:
	YCbCrFormat ycbcr_format;
};

}  // namespace movit

#endif // !defined(_MOVIT_YCBCR_CONVERSION_EFFECT_H)
