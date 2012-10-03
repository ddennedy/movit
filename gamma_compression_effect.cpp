#include <math.h>
#include <string.h>
#include <assert.h>

#include "gamma_compression_effect.h"
#include "util.h"

GammaCompressionEffect::GammaCompressionEffect()
	: destination_curve(GAMMA_LINEAR)
{
	register_int("destination_curve", (int *)&destination_curve);
	memset(compression_curve, 0, sizeof(compression_curve));
	register_1d_texture("compression_curve_tex", compression_curve, COMPRESSION_CURVE_SIZE);
}

std::string GammaCompressionEffect::output_fragment_shader()
{
	if (destination_curve == GAMMA_LINEAR) {
		return read_file("identity.frag");
	}
	if (destination_curve == GAMMA_sRGB) {
		for (unsigned i = 0; i < COMPRESSION_CURVE_SIZE; ++i) {
			float x = i / (float)(COMPRESSION_CURVE_SIZE - 1);
			if (x < 0.0031308f) {
				compression_curve[i] = 12.92f * x;
			} else {
				compression_curve[i] = 1.055f * pow(x, 1.0f / 2.4f) - 0.055f;
			}
		}
		invalidate_1d_texture("compression_curve_tex");
		return read_file("gamma_compression_effect.frag");
	}
	if (destination_curve == GAMMA_REC_709) {  // And Rec. 601.
		for (unsigned i = 0; i < COMPRESSION_CURVE_SIZE; ++i) {
			float x = i / (float)(COMPRESSION_CURVE_SIZE - 1);
			if (x < 0.018f) {
				compression_curve[i] = 4.5f * x;
			} else {
				compression_curve[i] = 1.099f * pow(x, 0.45f) - 0.099;
			}
		}
		invalidate_1d_texture("compression_curve_tex");
		return read_file("gamma_compression_effect.frag");
	}
	assert(false);
}
