#include <assert.h>

#include "gamma_compression_effect.h"
#include "util.h"

GammaCompressionEffect::GammaCompressionEffect()
	: destination_curve(GAMMA_LINEAR)
{
	register_int("destination_curve", (int *)&destination_curve);
}

std::string GammaCompressionEffect::output_fragment_shader()
{
	switch (destination_curve) {
	case GAMMA_LINEAR:
		return read_file("identity.frag");
	case GAMMA_sRGB:
		return read_file("gamma_compression_effect_srgb.frag");
	case GAMMA_REC_709:  // and GAMMA_REC_601
		return read_file("gamma_compression_effect_rec709.frag");
	default:
		assert(false);
	}
}
