// Note: These functions are tested in ycbcr_input_test.cpp; both through some
// direct matrix tests, but most of all through YCbCrInput's unit tests.

#include <Eigen/Core>
#include <Eigen/LU>

#include "ycbcr.h"

using namespace Eigen;

namespace movit {

// OpenGL has texel center in (0.5, 0.5), but different formats have
// chroma in various other places. If luma samples are X, the chroma
// sample is *, and subsampling is 3x3, the situation with chroma
// center in (0.5, 0.5) looks approximately like this:
//
//   X   X
//     *   
//   X   X
//
// If, on the other hand, chroma center is in (0.0, 0.5) (common
// for e.g. MPEG-4), the figure changes to:
//
//   X   X
//   *      
//   X   X
//
// In other words, (0.0, 0.0) means that the chroma sample is exactly
// co-sited on top of the top-left luma sample. Note, however, that
// this is _not_ 0.5 texels to the left, since the OpenGL's texel center
// is in (0.5, 0.5); it is in (0.25, 0.25). In a sense, the four luma samples
// define a square where chroma position (0.0, 0.0) is in texel position
// (0.25, 0.25) and chroma position (1.0, 1.0) is in texel position (0.75, 0.75)
// (the outer border shows the borders of the texel itself, ie. from
// (0, 0) to (1, 1)):
//
//  ---------
// |         |
// |  X---X  |
// |  | * |  |
// |  X---X  |
// |         |
//  ---------
//
// Also note that if we have no subsampling, the square will have zero
// area and the chroma position does not matter at all.
float compute_chroma_offset(float pos, unsigned subsampling_factor, unsigned resolution)
{
	float local_chroma_pos = (0.5 + pos * (subsampling_factor - 1)) / subsampling_factor;
	if (fabs(local_chroma_pos - 0.5) < 1e-10) {
		// x + (-0) can be optimized away freely, as opposed to x + 0.
		return -0.0;
	} else {
		return (0.5 - local_chroma_pos) / resolution;
	}
}

// Given <ycbcr_format>, compute the values needed to turn Y'CbCr into R'G'B';
// first subtract the returned offset, then left-multiply the returned matrix
// (the scaling is already folded into it).
void compute_ycbcr_matrix(YCbCrFormat ycbcr_format, float* offset, Matrix3d* ycbcr_to_rgb, GLenum type, double *scale_factor)
{
	double coeff[3], scale[3];

	switch (ycbcr_format.luma_coefficients) {
	case YCBCR_REC_601:
		// Rec. 601, page 2.
		coeff[0] = 0.299;
		coeff[1] = 0.587;
		coeff[2] = 0.114;
		break;

	case YCBCR_REC_709:
		// Rec. 709, page 19.
		coeff[0] = 0.2126;
		coeff[1] = 0.7152;
		coeff[2] = 0.0722;
		break;

	case YCBCR_REC_2020:
		// Rec. 2020, page 4.
		coeff[0] = 0.2627;
		coeff[1] = 0.6780;
		coeff[2] = 0.0593;
		break;

	default:
		assert(false);
	}

	int num_levels = ycbcr_format.num_levels;
	if (num_levels == 0) {
		// For the benefit of clients using old APIs, but still zeroing out the structure.
		num_levels = 256;
	}
	if (ycbcr_format.full_range) {
		offset[0] = 0.0 / (num_levels - 1);
		offset[1] = double(num_levels / 2) / (num_levels - 1);  // E.g. 128/255.
		offset[2] = double(num_levels / 2) / (num_levels - 1);

		scale[0] = 1.0;
		scale[1] = 1.0;
		scale[2] = 1.0;
	} else {
		// Rec. 601, page 4; Rec. 709, page 19; Rec. 2020, page 5.
		// Rec. 2020 contains the most generic formulas, which we use here.
		const double s = num_levels / 256.0;  // 2^(n-8) in Rec. 2020 parlance.
		offset[0] = (s * 16.0) / (num_levels - 1);
		offset[1] = (s * 128.0) / (num_levels - 1);
		offset[2] = (s * 128.0) / (num_levels - 1);

		scale[0] = double(num_levels - 1) / (s * 219.0);
		scale[1] = double(num_levels - 1) / (s * 224.0);
		scale[2] = double(num_levels - 1) / (s * 224.0);
	}

	// Matrix to convert RGB to YCbCr. See e.g. Rec. 601.
	Matrix3d rgb_to_ycbcr;
	rgb_to_ycbcr(0,0) = coeff[0];
	rgb_to_ycbcr(0,1) = coeff[1];
	rgb_to_ycbcr(0,2) = coeff[2];

	float cb_fac = 1.0 / (coeff[0] + coeff[1] + 1.0f - coeff[2]);
	rgb_to_ycbcr(1,0) = -coeff[0] * cb_fac;
	rgb_to_ycbcr(1,1) = -coeff[1] * cb_fac;
	rgb_to_ycbcr(1,2) = (1.0f - coeff[2]) * cb_fac;

	float cr_fac = 1.0 / (1.0f - coeff[0] + coeff[1] + coeff[2]);
	rgb_to_ycbcr(2,0) = (1.0f - coeff[0]) * cr_fac;
	rgb_to_ycbcr(2,1) = -coeff[1] * cr_fac;
	rgb_to_ycbcr(2,2) = -coeff[2] * cr_fac;

	// Inverting the matrix gives us what we need to go from YCbCr back to RGB.
	*ycbcr_to_rgb = rgb_to_ycbcr.inverse();

	// Fold in the scaling.
	*ycbcr_to_rgb *= Map<const Vector3d>(scale).asDiagonal();

	if (type == GL_UNSIGNED_SHORT) {
		// For 10-bit or 12-bit packed into 16-bit, we need to scale the values
		// so that the max value goes from 1023 (or 4095) to 65535. We do this
		// by folding the scaling into the conversion matrix, so it comes essentially
		// for free. However, the offset is before the scaling (and thus assumes
		// correctly scaled values), so we need to adjust that the other way.
		double scale = 65535.0 / (ycbcr_format.num_levels - 1);
		offset[0] /= scale;
		offset[1] /= scale;
		offset[2] /= scale;
		*ycbcr_to_rgb *= scale;
		if (scale_factor != nullptr) {
			*scale_factor = scale;
		}
	} else if (scale_factor != nullptr) {
		*scale_factor = 1.0;
	}
}

}  // namespace movit
