#include <math.h>
#include <assert.h>

#include "gamma_expansion_effect.h"
#include "util.h"

GammaExpansionEffect::GammaExpansionEffect()
	: source_curve(GAMMA_LINEAR)
{
	register_int("source_curve", (int *)&source_curve);
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
	if (source_curve == GAMMA_REC_709) {  // And Rec. 601.
		for (unsigned i = 0; i < EXPANSION_CURVE_SIZE; ++i) {
			float x = i / (float)(EXPANSION_CURVE_SIZE - 1);
			if (x < 0.081f) {
				expansion_curve[i] = (1.0/4.5f) * x;
			} else {
				expansion_curve[i] = pow((x + 0.099) * (1.0/1.099f), 1.0f/0.45f);
			}
		}
		invalidate_1d_texture("expansion_curve_tex");
		return read_file("gamma_expansion_effect.frag");
	}
	assert(false);
}
