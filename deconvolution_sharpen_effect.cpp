// NOTE: Throughout, we use the symbol ⊙ for convolution.
// Since all of our signals are symmetrical, discrete correlation and convolution
// is the same operation, and so we won't make a difference in notation.

#include <Eigen/Dense>
#include <Eigen/Cholesky>
#include <epoxy/gl.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <new>

#include "deconvolution_sharpen_effect.h"
#include "effect_util.h"
#include "util.h"

using namespace Eigen;
using namespace std;

namespace movit {

DeconvolutionSharpenEffect::DeconvolutionSharpenEffect()
	: R(5),
	  circle_radius(2.0f),
	  gaussian_radius(0.0f),
	  correlation(0.95f),
	  noise(0.01f),
	  last_R(-1),
	  last_circle_radius(-1.0f),
	  last_gaussian_radius(-1.0f),
	  last_correlation(-1.0f),
	  last_noise(-1.0f),
	  uniform_samples(nullptr)
{
	register_int("matrix_size", &R);
	register_float("circle_radius", &circle_radius);
	register_float("gaussian_radius", &gaussian_radius);
	register_float("correlation", &correlation);
	register_float("noise", &noise);
}

DeconvolutionSharpenEffect::~DeconvolutionSharpenEffect()
{
	delete[] uniform_samples;
}

string DeconvolutionSharpenEffect::output_fragment_shader()
{
	char buf[256];
	sprintf(buf, "#define R %u\n", R);

	assert(R >= 1);
	assert(R <= 25);  // Same limit as Refocus.

	uniform_samples = new float[4 * (R + 1) * (R + 1)];
	register_uniform_vec4_array("samples", uniform_samples, (R + 1) * (R + 1));

	last_R = R;
	return buf + read_file("deconvolution_sharpen_effect.frag");
}

namespace {

// Integral of sqrt(r² - x²) dx over x=0..a.
float circle_integral(float a, float r)
{
	assert(a >= 0.0f);
	if (a <= 0.0f) {
		return 0.0f;
	}
	if (a >= r) {
		return 0.25f * M_PI * r * r;
	}
	return 0.5f * (a * sqrt(r*r - a*a) + r*r * asin(a / r));
}

// Yields the impulse response of a circular blur with radius r.
// We basically look at each element as a square centered around (x,y),
// and figure out how much of its area is covered by the circle.
float circle_impulse_response(int x, int y, float r)
{
	if (r < 1e-3) {
		// Degenerate case: radius = 0 yields the impulse response.
		return (x == 0 && y == 0) ? 1.0f : 0.0f;
	}

	// Find the extents of this cell. Due to symmetry, we can cheat a bit
	// and pretend we're always in the upper-right quadrant, except when
	// we're right at an axis crossing (x = 0 or y = 0), in which case we
	// simply use the evenness of the function; shrink the cell, make
	// the calculation, and down below we'll normalize by the cell's area.
	float min_x, max_x, min_y, max_y;
	if (x == 0) {
		min_x = 0.0f;
		max_x = 0.5f;
	} else {
		min_x = abs(x) - 0.5f;
		max_x = abs(x) + 0.5f;
	}
	if (y == 0) {
		min_y = 0.0f;
		max_y = 0.5f;
	} else {
		min_y = abs(y) - 0.5f;
		max_y = abs(y) + 0.5f;
	}
	assert(min_x >= 0.0f && max_x >= 0.0f);
	assert(min_y >= 0.0f && max_y >= 0.0f);

	float cell_height = max_y - min_y;
	float cell_width = max_x - min_x;

	if (min_x * min_x + min_y * min_y > r * r) {
		// Lower-left corner is outside the circle, so the entire cell is.
		return 0.0f;
	}
	if (max_x * max_x + max_y * max_y < r * r) {
		// Upper-right corner is inside the circle, so the entire cell is.
		return 1.0f;
	}

	// OK, so now we know the cell is partially covered by the circle:
	//
	//      \           .
	//  -------------
	// |####|#\      |
	// |####|##|     |
	//  -------------
	//   A   ###|
	//       ###|
	//
	// The edge of the circle is defined by x² + y² = r², 
	// or x = sqrt(r² - y²) (since x is nonnegative).
	// Find out where the curve crosses our given y values.
	float mid_x1 = (max_y >= r) ? min_x : sqrt(r * r - max_y * max_y);
	float mid_x2 = sqrt(r * r - min_y * min_y);
	if (mid_x1 < min_x) {
		mid_x1 = min_x;
	}
	if (mid_x2 > max_x) {
		mid_x2 = max_x;
	}
	assert(mid_x1 >= min_x);
	assert(mid_x2 >= mid_x1);
	assert(max_x >= mid_x2);

	// The area marked A in the figure above.
	float covered_area = cell_height * (mid_x1 - min_x);

	// The area marked B in the figure above. Note that the integral gives the entire
	// shaded space down to zero, so we need to subtract the rectangle that does not
	// belong to our cell.
	covered_area += circle_integral(mid_x2, r) - circle_integral(mid_x1, r);
	covered_area -= min_y * (mid_x2 - mid_x1);

	assert(covered_area <= cell_width * cell_height);
	return covered_area / (cell_width * cell_height);
}

// Compute a ⊙ b. Note that we compute the “full” convolution,
// ie., our matrix will be big enough to hold every nonzero element of the result.
MatrixXf convolve(const MatrixXf &a, const MatrixXf &b)
{
	MatrixXf result(a.rows() + b.rows() - 1, a.cols() + b.cols() - 1);
	for (int yr = 0; yr < result.rows(); ++yr) {
		for (int xr = 0; xr < result.cols(); ++xr) {
			float sum = 0.0f;

			// Given that x_b = x_r - x_a, find the values of x_a where
			// x_a is in [0, a_cols> and x_b is in [0, b_cols>. (y is similar.)
			//
			// The second demand gives:
			//
			//   0 <= x_r - x_a < b_cols
			//   0 >= x_a - x_r > -b_cols
			//   x_r >= x_a > x_r - b_cols
			int ya_min = yr - b.rows() + 1;
			int ya_max = yr;
			int xa_min = xr - b.rows() + 1;
			int xa_max = xr;

			// Now fit to the first demand.
			ya_min = max<int>(ya_min, 0);
			ya_max = min<int>(ya_max, a.rows() - 1);
			xa_min = max<int>(xa_min, 0);
			xa_max = min<int>(xa_max, a.cols() - 1);

			assert(ya_max >= ya_min);
			assert(xa_max >= xa_min);

			for (int ya = ya_min; ya <= ya_max; ++ya) {
				for (int xa = xa_min; xa <= xa_max; ++xa) {
					sum += a(ya, xa) * b(yr - ya, xr - xa);
				}
			}

			result(yr, xr) = sum;
		}
	}
	return result;
}

// Similar to convolve(), but instead of assuming every element outside
// of b is zero, we make no such assumption and instead return only the
// elements where we know the right answer. (This is the only difference
// between the two.)
// This is the same as conv2(a, b, 'valid') in Octave.
//
// a must be the larger matrix of the two.
MatrixXf central_convolve(const MatrixXf &a, const MatrixXf &b)
{
	assert(a.rows() >= b.rows());
	assert(a.cols() >= b.cols());
	MatrixXf result(a.rows() - b.rows() + 1, a.cols() - b.cols() + 1);
	for (int yr = b.rows() - 1; yr < result.rows() + b.rows() - 1; ++yr) {
		for (int xr = b.cols() - 1; xr < result.cols() + b.cols() - 1; ++xr) {
			float sum = 0.0f;

			// Given that x_b = x_r - x_a, find the values of x_a where
			// x_a is in [0, a_cols> and x_b is in [0, b_cols>. (y is similar.)
			//
			// The second demand gives:
			//
			//   0 <= x_r - x_a < b_cols
			//   0 >= x_a - x_r > -b_cols
			//   x_r >= x_a > x_r - b_cols
			int ya_min = yr - b.rows() + 1;
			int ya_max = yr;
			int xa_min = xr - b.rows() + 1;
			int xa_max = xr;

			// Now fit to the first demand.
			ya_min = max<int>(ya_min, 0);
			ya_max = min<int>(ya_max, a.rows() - 1);
			xa_min = max<int>(xa_min, 0);
			xa_max = min<int>(xa_max, a.cols() - 1);

			assert(ya_max >= ya_min);
			assert(xa_max >= xa_min);

			for (int ya = ya_min; ya <= ya_max; ++ya) {
				for (int xa = xa_min; xa <= xa_max; ++xa) {
					sum += a(ya, xa) * b(yr - ya, xr - xa);
				}
			}

			result(yr - b.rows() + 1, xr - b.cols() + 1) = sum;
		}
	}
	return result;
}

}  // namespace

void DeconvolutionSharpenEffect::update_deconvolution_kernel()
{
	// Figure out the impulse response for the circular part of the blur.
	MatrixXf circ_h(2 * R + 1, 2 * R + 1);
	for (int y = -R; y <= R; ++y) {	
		for (int x = -R; x <= R; ++x) {
			circ_h(y + R, x + R) = circle_impulse_response(x, y, circle_radius);
		}
	}

	// Same, for the Gaussian part of the blur. We make this a lot larger
	// since we're going to convolve with it soon, and it has infinite support
	// (see comments for central_convolve()).
	MatrixXf gaussian_h(4 * R + 1, 4 * R + 1);
	for (int y = -2 * R; y <= 2 * R; ++y) {	
		for (int x = -2 * R; x <= 2 * R; ++x) {
			float val;
			if (gaussian_radius < 1e-3) {
				val = (x == 0 && y == 0) ? 1.0f : 0.0f;
			} else {
				val = exp(-(x*x + y*y) / (2.0 * gaussian_radius * gaussian_radius));
			}
			gaussian_h(y + 2 * R, x + 2 * R) = val;
		}
	}

	// h, the (assumed) impulse response that we're trying to invert.
	MatrixXf h = central_convolve(gaussian_h, circ_h);
	assert(h.rows() == 2 * R + 1);
	assert(h.cols() == 2 * R + 1);

	// Normalize the impulse response.
	float sum = 0.0f;
	for (int y = 0; y < 2 * R + 1; ++y) {
		for (int x = 0; x < 2 * R + 1; ++x) {
			sum += h(y, x);
		}
	}
	for (int y = 0; y < 2 * R + 1; ++y) {
		for (int x = 0; x < 2 * R + 1; ++x) {
			h(y, x) /= sum;
		}
	}

	// r_uu, the (estimated/assumed) autocorrelation of the input signal (u).
	// The signal is modelled a standard autoregressive process with the
	// given correlation coefficient.
	//
	// We have to take a bit of care with the size of this matrix.
	// The pow() function naturally has an infinite support (except for the
	// degenerate case of correlation=0), but we have to chop it off
	// somewhere. Since we convolve it with a 4*R+1 large matrix below,
	// we need to make it twice as big as that, so that we have enough
	// data to make r_vv valid. (central_convolve() effectively enforces
	// that we get at least the right size.)
	MatrixXf r_uu(8 * R + 1, 8 * R + 1);
	for (int y = -4 * R; y <= 4 * R; ++y) {	
		for (int x = -4 * R; x <= 4 * R; ++x) {
			r_uu(x + 4 * R, y + 4 * R) = pow(double(correlation), hypot(x, y));
		}
	}

	// Estimate r_vv, the autocorrelation of the output signal v.
	// Since we know that v = h ⊙ u and both are symmetrical,
	// convolution and correlation are the same, and
	// r_vv = v ⊙ v = (h ⊙ u) ⊙ (h ⊙ u) = (h ⊙ h) ⊙ r_uu.
	MatrixXf r_vv = central_convolve(r_uu, convolve(h, h));
	assert(r_vv.rows() == 4 * R + 1);
	assert(r_vv.cols() == 4 * R + 1);

	// Similarly, r_uv = u ⊙ v = u ⊙ (h ⊙ u) = h ⊙ r_uu.
	MatrixXf r_uu_center = r_uu.block(2 * R, 2 * R, 4 * R + 1, 4 * R + 1);
	MatrixXf r_uv = central_convolve(r_uu_center, h);
	assert(r_uv.rows() == 2 * R + 1);
	assert(r_uv.cols() == 2 * R + 1);
	
	// Add the noise term (we assume the noise is uncorrelated,
	// so it only affects the central element).
	r_vv(2 * R, 2 * R) += noise;

	// Now solve the Wiener-Hopf equations to find the deconvolution kernel g.
	// Most texts show this only for the simpler 1D case:
	//
	// [ r_vv(0)  r_vv(1) r_vv(2) ... ] [ g(0) ]   [ r_uv(0) ]
	// [ r_vv(-1) r_vv(0) ...         ] [ g(1) ] = [ r_uv(1) ]
	// [ r_vv(-2) ...                 ] [ g(2) ]   [ r_uv(2) ]
	// [ ...                          ] [ g(3) ]   [ r_uv(3) ]
	//
	// (Since r_vv is symmetrical, we can drop the minus signs.)
	//
	// Generally, row i of the matrix contains (dropping _vv for brevity):
	//
	// [ r(0-i) r(1-i) r(2-i) ... ]
	//
	// However, we have the 2D case. We flatten the vectors out to
	// 1D quantities; this means we must think of the row number
	// as a pair instead of as a scalar. Row (i,j) then contains:
	//
	// [ r(0-i,0-j) r(1-i,0-j) r(2-i,0-j) ... r(0-i,1-j) r_(1-i,1-j) r(2-i,1-j) ... ]
	//
	// g and r_uv are flattened in the same fashion.
	//
	// Note that even though this matrix is block Toeplitz, it is _not_ Toeplitz,
	// and thus can not be inverted through the standard Levinson-Durbin method.
	// There exists a block Levinson-Durbin method, which we may or may not
	// want to use later. (Eigen's solvers are fast enough that for big matrices,
	// the convolution operation and not the matrix solving is the bottleneck.)
	//
	// One thing we definitely want to use, though, is the symmetry properties.
	// Since we know that g(i, j) = g(|i|, |j|), we can reduce the amount of
	// unknowns to about 1/4th of the total size. The method is quite simple,
	// as can be seen from the following toy equation system:
	//
	//   A x0 + B x1 + C x2 = y0
	//   D x0 + E x1 + F x2 = y1
	//   G x0 + H x1 + I x2 = y2
	//
	// If we now know that e.g. x0=x1 and y0=y1, we can rewrite this to
	//
	//   (A+B+D+E) x0 + (C+F) x2 = 2 y0
	//   (G+H)     x0 + I x2     = y2
	//
	// This both increases accuracy and provides us with a very nice speed
	// boost.
	MatrixXf M(MatrixXf::Zero((R + 1) * (R + 1), (R + 1) * (R + 1)));
	MatrixXf r_uv_flattened(MatrixXf::Zero((R + 1) * (R + 1), 1));
	for (int outer_i = 0; outer_i < 2 * R + 1; ++outer_i) {
		int folded_outer_i = abs(outer_i - R);
		for (int outer_j = 0; outer_j < 2 * R + 1; ++outer_j) {
			int folded_outer_j = abs(outer_j - R);
			int row = folded_outer_i * (R + 1) + folded_outer_j;
			for (int inner_i = 0; inner_i < 2 * R + 1; ++inner_i) {
				int folded_inner_i = abs(inner_i - R);
				for (int inner_j = 0; inner_j < 2 * R + 1; ++inner_j) {
					int folded_inner_j = abs(inner_j - R);
					int col = folded_inner_i * (R + 1) + folded_inner_j;
					M(row, col) += r_vv((inner_i - R) - (outer_i - R) + 2 * R,
					                    (inner_j - R) - (outer_j - R) + 2 * R);
				}
			}
			r_uv_flattened(row) += r_uv(outer_i, outer_j);
		}
	}

	LLT<MatrixXf> llt(M);
	MatrixXf g_flattened = llt.solve(r_uv_flattened);
	assert(g_flattened.rows() == (R + 1) * (R + 1)),
	assert(g_flattened.cols() == 1);

	// Normalize and de-flatten the deconvolution matrix.
	g = MatrixXf(R + 1, R + 1);
	sum = 0.0f;
	for (int i = 0; i < g_flattened.rows(); ++i) {
		int y = i / (R + 1);
		int x = i % (R + 1);
		if (y == 0 && x == 0) {
			sum += g_flattened(i);
		} else if (y == 0 || x == 0) {
			sum += 2.0f * g_flattened(i);
		} else {
			sum += 4.0f * g_flattened(i);
		}
	}
	for (int i = 0; i < g_flattened.rows(); ++i) {
		int y = i / (R + 1);
		int x = i % (R + 1);
		g(y, x) = g_flattened(i) / sum;
	}

	last_circle_radius = circle_radius;
	last_gaussian_radius = gaussian_radius;
	last_correlation = correlation;
	last_noise = noise;
}

void DeconvolutionSharpenEffect::set_gl_state(GLuint glsl_program_num, const string &prefix, unsigned *sampler_num)
{
	Effect::set_gl_state(glsl_program_num, prefix, sampler_num);

	assert(R == last_R);

	if (fabs(circle_radius - last_circle_radius) > 1e-3 ||
	    fabs(gaussian_radius - last_gaussian_radius) > 1e-3 ||
	    fabs(correlation - last_correlation) > 1e-3 ||
	    fabs(noise - last_noise) > 1e-3) {
		update_deconvolution_kernel();
	}
	// Now encode it as uniforms, and pass it on to the shader.
	for (int y = 0; y <= R; ++y) {
		for (int x = 0; x <= R; ++x) {
			int i = y * (R + 1) + x;
			uniform_samples[i * 4 + 0] = x / float(width);
			uniform_samples[i * 4 + 1] = y / float(height);
			uniform_samples[i * 4 + 2] = g(y, x);
			uniform_samples[i * 4 + 3] = 0.0f;
		}
	}
}

}  // namespace movit
