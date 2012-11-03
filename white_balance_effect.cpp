#include <math.h>
#include <assert.h>

#include <Eigen/LU>

#include "white_balance_effect.h"
#include "util.h"
#include "opengl.h"
#include "d65.h"

using namespace Eigen;

namespace {

// Temperature is in Kelvin. Formula from http://en.wikipedia.org/wiki/Planckian_locus#Approximation .
Vector3d convert_color_temperature_to_xyz(float T)
{
	double invT = 1.0 / T;
	double x, y;

	assert(T >= 1000.0f);
	assert(T <= 15000.0f);

	if (T <= 4000.0f) {
		x = ((-0.2661239e9 * invT - 0.2343580e6) * invT + 0.8776956e3) * invT + 0.179910;
	} else {
		x = ((-3.0258469e9 * invT + 2.1070379e6) * invT + 0.2226347e3) * invT + 0.240390;
	}

	if (T <= 2222.0f) {
		y = ((-1.1063814 * x - 1.34811020) * x + 2.18555832) * x - 0.20219683;
	} else if (T <= 4000.0f) {
		y = ((-0.9549476 * x - 1.37418593) * x + 2.09137015) * x - 0.16748867;
	} else {
		y = (( 3.0817580 * x - 5.87338670) * x + 3.75112997) * x - 0.37001483;
	}

	return Vector3d(x, y, 1.0 - x - y);
}

// Assuming sRGB primaries, from Wikipedia.
double rgb_to_xyz_matrix[9] = {
	0.4124, 0.2126, 0.0193, 
	0.3576, 0.7152, 0.1192,
	0.1805, 0.0722, 0.9505,
};

/*
 * There are several different perceptual color spaces with different intended
 * uses; for instance, CIECAM02 uses one space (CAT02) for purposes of computing
 * chromatic adaptation (the effect that the human eye perceives an object as
 * the same color even under differing illuminants), but a different space
 * (Hunt-Pointer-Estevez, or HPE) for the actual perception post-adaptation. 
 *
 * CIECAM02 chromatic adaptation, while related to the transformation we want,
 * is a more complex phenomenon that depends on factors like the total luminance
 * (in cd/m²) of the illuminant, and can no longer be implemented by just scaling
 * each component in LMS space linearly. The simpler way out is to use the HPE matrix,
 * which is intended to be close to the actual cone response; this results in
 * the “von Kries transformation” when we couple it with normalization in LMS space.
 *
 * http://www.brucelindbloom.com/index.html?Eqn_ChromAdapt.html compares
 * von Kries transformation with using another matrix, the Bradford matrix,
 * and generally finds that the Bradford method gives a better result,
 * as in giving better matches with the true result (as calculated using
 * spectral matching) when converting between various CIE illuminants.
 * The actual perceptual differences were found to be minor, though.
 * We use the Bradford tranformation matrix from that page, and compute the
 * inverse ourselves. (The Bradford matrix is also used in CMCCAT97.) 
 *
 * We normalize the Bradford fundamentals to D65, which means that the standard
 * D65 illuminant (x=0.31271, y=0.32902, z=1-y-x) gives L=M=S under this
 * transformation. This makes sense because sRGB (which is used to derive
 * those XYZ values in the first place) assumes the D65 illuminant, and so the
 * D65 illuminant also gives R=G=B in sRGB. (We could also have done this
 * step separately in XYZ space, but we'd have to do it to all colors we
 * wanted scaled to LMS.)
 */
const double xyz_to_lms_matrix[9] = {
	 0.8951 / d65_X, -0.7502 / d65_X,  0.0389 / d65_X,
	 0.2664,          1.7135,         -0.0685,
	-0.1614 / d65_Z,  0.0367 / d65_Z,  1.0296 / d65_Z,
};

/*
 * For a given reference color (given in XYZ space),
 * compute scaling factors for L, M and S. What we want at the output is equal L, M and S
 * for the reference color (making it a neutral illuminant), or sL ref_L = sM ref_M = sS ref_S.
 * This removes two degrees of freedom for our system, and we only need to find fL.
 *
 * A reasonable last constraint would be to preserve Y, approximately the brightness,
 * for the reference color. Since L'=M'=S' and the Y row of the LMS-to-XYZ matrix
 * sums to unity, we know that Y'=L', and it's easy to find the fL that sets Y'=Y.
 */
Vector3d compute_lms_scaling_factors(const Vector3d &xyz)
{
	Vector3d lms = Map<const Matrix3d>(xyz_to_lms_matrix) * xyz;
	double l = lms[0];
	double m = lms[1];
	double s = lms[2];

	double scale_l = xyz[1] / l;
	double scale_m = scale_l * (l / m);
	double scale_s = scale_l * (l / s);

	return Vector3d(scale_l, scale_m, scale_s);
}

}  // namespace

WhiteBalanceEffect::WhiteBalanceEffect()
	: neutral_color(0.5f, 0.5f, 0.5f),
	  output_color_temperature(6500.0f)
{
	register_vec3("neutral_color", (float *)&neutral_color);
	register_float("output_color_temperature", &output_color_temperature);
}

std::string WhiteBalanceEffect::output_fragment_shader()
{
	return read_file("white_balance_effect.frag");
}

void WhiteBalanceEffect::set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num)
{
	Vector3d rgb(neutral_color.r, neutral_color.g, neutral_color.b);
	Vector3d xyz = Map<const Matrix3d>(rgb_to_xyz_matrix) * rgb;
	Vector3d lms_scale = compute_lms_scaling_factors(xyz);

	/*
	 * Now apply the color balance. Simply put, we find the chromacity point
	 * for the desired white temperature, see what LMS scaling factors they
	 * would have given us, and then reverse that transform. For T=6500K,
	 * the default, this gives us nearly an identity transform (but only nearly,
	 * since the D65 illuminant does not exactly match the results of T=6500K);
	 * we normalize so that T=6500K really is a no-op.
	 */
	Vector3d white_xyz = convert_color_temperature_to_xyz(output_color_temperature);
	Vector3d lms_scale_white = compute_lms_scaling_factors(white_xyz);

	Vector3d ref_xyz = convert_color_temperature_to_xyz(6500.0f);
	Vector3d lms_scale_ref = compute_lms_scaling_factors(ref_xyz);

	lms_scale[0] *= lms_scale_ref[0] / lms_scale_white[0];
	lms_scale[1] *= lms_scale_ref[1] / lms_scale_white[1];
	lms_scale[2] *= lms_scale_ref[2] / lms_scale_white[2];

	/*
	 * Concatenate all the different linear operations into a single 3x3 matrix.
	 * Note that since we postmultiply our vectors, the order of the matrices
	 * has to be the opposite of the execution order.
	 */
	Matrix3d corr_matrix =
		Map<const Matrix3d>(rgb_to_xyz_matrix).inverse() *
		Map<const Matrix3d>(xyz_to_lms_matrix).inverse() *
		lms_scale.asDiagonal() *
		Map<const Matrix3d>(xyz_to_lms_matrix) *
		Map<const Matrix3d>(rgb_to_xyz_matrix);
	set_uniform_mat3(glsl_program_num, prefix, "correction_matrix", corr_matrix);
}
