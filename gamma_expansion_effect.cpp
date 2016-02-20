#include <assert.h>

#include "effect_util.h"
#include "gamma_expansion_effect.h"
#include "util.h"

using namespace std;

namespace movit {

GammaExpansionEffect::GammaExpansionEffect()
	: source_curve(GAMMA_LINEAR)
{
	register_int("source_curve", (int *)&source_curve);
	register_uniform_float("linear_scale", &uniform_linear_scale);
	register_uniform_float_array("c", uniform_c, 5);
	register_uniform_float("beta", &uniform_beta);
}

string GammaExpansionEffect::output_fragment_shader()
{
	if (source_curve == GAMMA_LINEAR) {
		return read_file("identity.frag");
	}
	if (source_curve == GAMMA_sRGB ||
	    source_curve == GAMMA_REC_709 ||  // Also includes Rec. 601, and 10-bit Rec. 2020.
	    source_curve == GAMMA_REC_2020_12_BIT) {
		return read_file("gamma_expansion_effect.frag");
	}
	assert(false);
}

void GammaExpansionEffect::set_gl_state(GLuint glsl_program_num, const string &prefix, unsigned *sampler_num)
{
	Effect::set_gl_state(glsl_program_num, prefix, sampler_num);

	// All of these curves follow a continuous curve that's piecewise defined;
	// very low values (up to some β) are linear. Above β, we have a power curve
	// that looks like this:
	//
	//   y = ((x + ɑ - 1) / ɑ)^ɣ
	//
	// However, pow() is relatively slow in GLSL, so we approximate this
	// part by a minimax polynomial, whose coefficients are precalculated
	// in Maple. (It is very hard to accurately model the curve as a whole
	// using minimax polynomials; both Maple and Mathematica generally
	// just error out if you ask them to optimize over 0..1 with a higher-degree
	// polynomial.)
	//
	// We put some extra weight on areas near β to keep a continuous curve,
	// and near 1.0, since we'd really like f(1.0) = 1.0, or approximately so.
	// The following Maple commands, using sRGB below as an example, will
	// compute the coefficients:
	//
	// > alpha := 1.055;
	// > beta := 0.04045;
	// > gamma_ := 2.4;
	// > w := x -> piecewise(x < beta + 0.001, 10, x > 0.999, 10, 1);
	// > numapprox[minimax](((x + alpha - 1) / alpha)^gamma_, x=beta..1, [4,0], w(x), 'maxerror');
	//
	// The variable 'maxerror' will then contain the maximum absolute error
	// at any point of the curve, and we report this along with the absolute
	// error at beta and at 1.0. Keep in mind that along this curve,
	// the smallest minimum difference between any two 8-bit sRGB pixel levels
	// (in the exponential part of the curve) in linear light is that
	// between 11/255 and 12/255, which is about 0.00033 (or three to four
	// times of the sRGB maxerror below). The choice of a fourth-degree
	// polynomial was made with this in mind; we have not cared equally
	// much about 10- and 12-bit Rec. 2020.
	//
	// NOTE: The error at beta is compared to the _linear_ part of the curve.
	// Since the standards give these with only a few decimals, it means that
	// the linear and exponential parts will not match up exactly, and even
	// a perfect approximation will have error > 0 here; sometimes, even larger
	// than maxerror for the curve itself.

	if (source_curve == GAMMA_sRGB) {
		// From the Wikipedia article on sRGB; ɑ (called a+1 there) = 1.055,
		// β = 0.04045, ɣ = 2.4.
		// maxerror      = 0.000094
		// error at beta = 0.000012
		// error at 1.0  = 0.000012
		//
		// Note that the worst _relative_ error by far is just at the beginning
		// of the exponential curve, ie., just around β.
		uniform_linear_scale = 1.0 / 12.92;
		uniform_c[0] = 0.001324469581;
		uniform_c[1] = 0.02227416690;
		uniform_c[2] = 0.5917615253;
		uniform_c[3] = 0.4733532353;
		uniform_c[4] = -0.08880738120;
		uniform_beta = 0.04045;
	}
	if (source_curve == GAMMA_REC_709) {  // Also includes Rec. 601, and 10-bit Rec. 2020.
		// Rec. 2020, page 3; ɑ = 1.099, β = 0.018 * 4.5, ɣ = 1/0.45.
		// maxerror      = 0.000043
		// error at beta = 0.000051 (see note above!)
		// error at 1.0  = 0.000004
		//
		// Note that Rec. 2020 only gives the other direction, which is why
		// our beta and gamma are different from the numbers mentioned
		// (we've inverted the formula).
		uniform_linear_scale = 1.0 / 4.5;
		uniform_c[0] = 0.005137028744;
		uniform_c[1] = 0.09802596889;
		uniform_c[2] = 0.7255768864;
		uniform_c[3] = 0.2135067966;
		uniform_c[4] = -0.04225094667;
		uniform_beta = 0.018 * 4.5;
	}
	if (source_curve == GAMMA_REC_2020_12_BIT) {
		// Rec. 2020, page 3; ɑ = 1.0993, β = 0.0181 * 4.5, ɣ = 1/0.45.
		// maxerror      = 0.000042
		// error at beta = 0.000005
		// error at 1.0  = 0.000004
		//
		// Note that Rec. 2020 only gives the other direction, which is why
		// our beta and gamma are different from the numbers mentioned
		// (we've inverted the formula).
		uniform_linear_scale = 1.0 / 4.5;
		uniform_c[0] = 0.005167545928;
		uniform_c[1] = 0.09835585809;
		uniform_c[2] = 0.7254820139;
		uniform_c[3] = 0.2131291155;
		uniform_c[4] = -0.04213877222;
		uniform_beta = 0.0181 * 4.5;
	}
}

}  // namespace movit
