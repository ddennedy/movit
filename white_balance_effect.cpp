#include <math.h>
#include <assert.h>

#include "white_balance_effect.h"
#include "util.h"
#include "opengl.h"

namespace {

// Temperature is in Kelvin. Formula from http://en.wikipedia.org/wiki/Planckian_locus#Approximation .
void convert_color_temperature_to_xyz(float T, float *x, float *y, float *z)
{
	double invT = 1.0 / T;
	double xc, yc;

	assert(T >= 1000.0f);
	assert(T <= 15000.0f);

	if (T <= 4000.0f) {
		xc = ((-0.2661239e9 * invT - 0.2343580e6) * invT + 0.8776956e3) * invT + 0.179910;
	} else {
		xc = ((-3.0258469e9 * invT + 2.1070379e6) * invT + 0.2226347e3) * invT + 0.240390;
	}

	if (T <= 2222.0f) {
		yc = ((-1.1063814 * xc - 1.34811020) * xc + 2.18555832) * xc - 0.20219683;
	} else if (T <= 4000.0f) {
		yc = ((-0.9549476 * xc - 1.37418593) * xc + 2.09137015) * xc - 0.16748867;
	} else {
		yc = (( 3.0817580 * xc - 5.87338670) * xc + 3.75112997) * xc - 0.37001483;
	}

	*x = xc;
	*y = yc;
	*z = 1.0 - xc - yc;
}

// Assuming sRGB primaries, from Wikipedia.
static const Matrix3x3 rgb_to_xyz_matrix = {
	0.4124, 0.2126, 0.0193, 
	0.3576, 0.7152, 0.1192,
	0.1805, 0.0722, 0.9505,
};

/*
 * There are several different LMS spaces, at least according to Wikipedia.
 * Through practical testing, I've found most of them (like the CIECAM02 model)
 * to yield a result that is too reddish in practice, possibly because they
 * are intended for different illuminants than what sRGB assumes. 
 *
 * This is the RLAB space, normalized to D65, which means that the standard
 * D65 illuminant (x=0.31271, y=0.32902, z=1-y-x) gives L=M=S under this transformation.
 * This makes sense because sRGB (which is used to derive those XYZ values
 * in the first place) assumes the D65 illuminant, and so the D65 illuminant
 * also gives R=G=B in sRGB.
 */
static const Matrix3x3 xyz_to_lms_matrix = {
	 0.4002, -0.2263,    0.0,
	 0.7076,  1.1653,    0.0,
	-0.0808,  0.0457, 0.9182,
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
static void compute_lms_scaling_factors(float x, float y, float z, float *scale_l, float *scale_m, float *scale_s)
{
	float l, m, s;
	multiply_3x3_matrix_float3(xyz_to_lms_matrix, x, y, z, &l, &m, &s);

	*scale_l = y / l;
	*scale_m = *scale_l * (l / m);
	*scale_s = *scale_l * (l / s);
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
	float x, y, z;
	multiply_3x3_matrix_float3(rgb_to_xyz_matrix, neutral_color.r, neutral_color.g, neutral_color.b, &x, &y, &z);

	float l, m, s;
	multiply_3x3_matrix_float3(xyz_to_lms_matrix, x, y, z, &l, &m, &s);

	float l_scale, m_scale, s_scale;
	compute_lms_scaling_factors(x, y, z, &l_scale, &m_scale, &s_scale);

	/*
	 * Now apply the color balance. Simply put, we find the chromacity point
	 * for the desired white temperature, see what LMS scaling factors they
	 * would have given us, and then reverse that transform. For T=6500K,
	 * the default, this gives us nearly an identity transform (but only nearly,
	 * since the D65 illuminant does not exactly match the results of T=6500K);
	 * we normalize so that T=6500K really is a no-op.
	 */
	float white_x, white_y, white_z, l_scale_white, m_scale_white, s_scale_white;
	convert_color_temperature_to_xyz(output_color_temperature, &white_x, &white_y, &white_z);
	compute_lms_scaling_factors(white_x, white_y, white_z, &l_scale_white, &m_scale_white, &s_scale_white);
	
	float ref_x, ref_y, ref_z, l_scale_ref, m_scale_ref, s_scale_ref;
	convert_color_temperature_to_xyz(6500.0f, &ref_x, &ref_y, &ref_z);
	compute_lms_scaling_factors(ref_x, ref_y, ref_z, &l_scale_ref, &m_scale_ref, &s_scale_ref);
	
	l_scale *= l_scale_ref / l_scale_white;
	m_scale *= m_scale_ref / m_scale_white;
	s_scale *= s_scale_ref / s_scale_white;
	
	/*
	 * Concatenate all the different linear operations into a single 3x3 matrix.
	 * Note that since we postmultiply our vectors, the order of the matrices
	 * has to be the opposite of the execution order.
	 */
	Matrix3x3 lms_to_xyz_matrix, xyz_to_rgb_matrix;
	invert_3x3_matrix(xyz_to_lms_matrix, lms_to_xyz_matrix);
	invert_3x3_matrix(rgb_to_xyz_matrix, xyz_to_rgb_matrix);

	Matrix3x3 temp, temp2, corr_matrix;
	Matrix3x3 lms_scale_matrix = {
		l_scale,    0.0f,    0.0f,
		   0.0f, m_scale,    0.0f,
		   0.0f,    0.0f, s_scale,
	};
	multiply_3x3_matrices(xyz_to_rgb_matrix, lms_to_xyz_matrix, temp);
	multiply_3x3_matrices(temp, lms_scale_matrix, temp2);
	multiply_3x3_matrices(temp2, xyz_to_lms_matrix, temp);
	multiply_3x3_matrices(temp, rgb_to_xyz_matrix, corr_matrix);

	set_uniform_mat3(glsl_program_num, prefix, "correction_matrix", corr_matrix);
}
