#include <assert.h>

#include "effect_util.h"
#include "gamma_compression_effect.h"
#include "util.h"

using namespace std;

namespace movit {

GammaCompressionEffect::GammaCompressionEffect()
	: destination_curve(GAMMA_LINEAR)
{
	register_int("destination_curve", (int *)&destination_curve);
	register_uniform_float("linear_scale", &uniform_linear_scale);
	register_uniform_float_array("c", uniform_c, 5);
	register_uniform_float("beta", &uniform_beta);
}

string GammaCompressionEffect::output_fragment_shader()
{
	if (destination_curve == GAMMA_LINEAR) {
		return read_file("identity.frag");
	}
	if (destination_curve == GAMMA_sRGB ||
	    destination_curve == GAMMA_REC_709 ||  // Also includes Rec. 601, and 10-bit Rec. 2020.
	    destination_curve == GAMMA_REC_2020_12_BIT) {
		return read_file("gamma_compression_effect.frag");
	}
	assert(false);
}

void GammaCompressionEffect::set_gl_state(GLuint glsl_program_num, const string &prefix, unsigned *sampler_num)
{
	Effect::set_gl_state(glsl_program_num, prefix, sampler_num);

	// See GammaExpansionEffect for more details about the approximations in use;
	// we will primarily deal with the differences here.
	//
	// Like in expansion, we have a piecewise curve that for very low values
	// (up to some β) are linear. Above β, we have a power curve that looks
	// like this:
	//
	//   y = ɑ x^ɣ - (ɑ - 1)
	//
	// Like in expansion, we want to approximate this by some minimax polynomial
	// in the range β..1. However, in this case, ɣ is typically around 0.4, and
	// x^0.4 is actually very hard to approximate accurately in this range.
	// We do a little trick by instead asking for a polynomial of s=sqrt(x),
	// which means we instead need something like s^0.8, which is much easier.
	// This warps the input space a bit as seen by the minimax algorithm,
	// but since we are optimizing for _maximum_ error and not _average_,
	// we should not add any extra weighting factors.
	//
	// However, since we have problems reaching the desired accuracy (~25%
	// of a pixel level), especially for sRGB, we modify w(x) from
	// GammaExpansionEffect to remove the special handling of the area
	// around β; it is not really as useful when the next step is just a
	// dither and round anyway. We keep it around 1, though, since that
	// seems to hurt less.
	//
	// The Maple commands this time around become (again using sRGB as an example):
	//
	// > alpha := 1.055;
	// > beta := 0.0031308;
	// > gamma_ := 1.0/2.4;
	// > w := x -> piecewise(x > 0.999, 10, 1);
	// > numapprox[minimax](alpha * (x^2)^gamma_ - (alpha - 1), x=sqrt(beta)..1, [4,0], w(x^2), 'maxerror');
	//
	// Since the error here is possible to interpret on a uniform scale,
	// we also show it as a value relative to a 8-, 10- or 12-bit pixel level,
	// as appropriate.

	if (destination_curve == GAMMA_sRGB) {
		// From the Wikipedia article on sRGB; ɑ (called a+1 there) = 1.055,
		// β = 0.0031308, ɣ = 1/2.4.
		// maxerror      = 0.000785 = 0.200 * 255
		// error at 1.0  = 0.000078 = 0.020 * 255
		uniform_linear_scale = 12.92;
		uniform_c[0] = -0.03679675939;
		uniform_c[1] = 1.443803073;
		uniform_c[2] = -0.9239780987;
		uniform_c[3] = 0.8060491596;
		uniform_c[4] = -0.2891558568;
		uniform_beta = 0.0031308;
	}
	if (destination_curve == GAMMA_REC_709) {  // Also includes Rec. 601, and 10-bit Rec. 2020.
		// Rec. 2020, page 3; ɑ = 1.099, β = 0.018, ɣ = 0.45.
		// maxerror      = 0.000131 = 0.033 * 255 = 0.134 * 1023
		// error at 1.0  = 0.000013 = 0.003 * 255 = 0.013 * 1023
		uniform_linear_scale = 4.5;
		uniform_c[0] = -0.08541688528;
		uniform_c[1] = 1.292793370;
		uniform_c[2] = -0.4070417645;
		uniform_c[3] = 0.2923891828;
		uniform_c[4] = -0.09273699351;
		uniform_beta = 0.018;
	}
	if (destination_curve == GAMMA_REC_2020_12_BIT) {
		// Rec. 2020, page 3; ɑ = 1.0993, β = 0.0181, ɣ = 0.45.
		// maxerror      = 0.000130 = 0.533 * 4095
		// error at 1.0  = 0.000013 = 0.053 * 4095
		//
		// Note that this error is above one half of a pixel level,
		// which means that a few values will actually be off in the lowest
		// bit. (Removing the constraint for x=1 will only take this down
		// from 0.553 to 0.501; adding a fifth order can get it down to
		// 0.167, although this assumes working in fp64 and not fp32.)
		uniform_linear_scale = 4.5;
		uniform_c[0] = -0.08569685663;
		uniform_c[1] = 1.293000900;
		uniform_c[2] = -0.4067291321;
		uniform_c[3] = 0.2919741179;
		uniform_c[4] = -0.09256205770;
		uniform_beta = 0.0181;
	}
}

}  // namespace movit
