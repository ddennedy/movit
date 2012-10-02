#include <assert.h>

#include "gamma_expansion_effect.h"
#include "util.h"

GammaExpansionEffect::GammaExpansionEffect()
	: source_curve(GAMMA_LINEAR)
{
	register_int("source_curve", (int *)&source_curve);
}

std::string GammaExpansionEffect::output_fragment_shader()
{
	switch (source_curve) {
	case GAMMA_LINEAR:
		return read_file("identity-fs.glsl");
	case GAMMA_sRGB:
		return read_file("gamma_expansion_effect_srgb.glsl");
	case GAMMA_REC_709:  // and GAMMA_REC_601
		return read_file("gamma_expansion_effect_rec709.glsl");
	default:
		assert(false);
	}
}
