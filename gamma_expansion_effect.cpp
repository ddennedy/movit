#include <math.h>
#include <assert.h>

#include "gamma_expansion_effect.h"
#include "util.h"

GammaExpansionEffect::GammaExpansionEffect()
	: source_curve(GAMMA_LINEAR)
{
	register_int("source_curve", (int *)&source_curve);
	memset(expansion_curve, 0, sizeof(expansion_curve));
	register_1d_texture("expansion_curve_tex", expansion_curve, EXPANSION_CURVE_SIZE);
}

std::string GammaExpansionEffect::output_fragment_shader()
{
	if (source_curve == GAMMA_LINEAR) {
		return read_file("identity.frag");
	}
	if (source_curve == GAMMA_sRGB) {
		for (unsigned i = 0; i < EXPANSION_CURVE_SIZE; ++i) {
			float x = i / (float)(EXPANSION_CURVE_SIZE - 1);
			if (x < 0.04045f) {
				expansion_curve[i] = (1.0/12.92f) * x;
			} else {
				expansion_curve[i] = pow((x + 0.055) * (1.0/1.055f), 2.4);
			}
		}
		invalidate_1d_texture("expansion_curve_tex");
		return read_file("gamma_expansion_effect.frag");
	}
	if (source_curve == GAMMA_REC_709 ||  // Also includes Rec. 601, and 10-bit Rec. 2020.
	    source_curve == GAMMA_REC_2020_12_BIT) {
		// Rec. 2020, page 3.
		float alpha, beta;
		if (source_curve == GAMMA_REC_2020_12_BIT) {
			alpha = 1.0993f;
			beta = 0.0181f;
		} else {
			alpha = 1.099f;
			beta = 0.018f;
		}
		for (unsigned i = 0; i < EXPANSION_CURVE_SIZE; ++i) {
			float x = i / (float)(EXPANSION_CURVE_SIZE - 1);
			if (x < beta * 4.5f) {
				expansion_curve[i] = (1.0/4.5f) * x;
			} else {
				expansion_curve[i] = pow((x + (alpha - 1.0f)) / alpha, 1.0f/0.45f);
			}
		}
		invalidate_1d_texture("expansion_curve_tex");
		return read_file("gamma_expansion_effect.frag");
	}
	assert(false);
}
