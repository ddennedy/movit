#ifndef _MOVIT_YCBCR_H
#define _MOVIT_YCBCR_H 1

// Shared utility functions between YCbCrInput and YCbCr422InterleavedInput.

#include "image_format.h"

#include <Eigen/Core>

namespace movit {

struct YCbCrFormat {
	// Which formula for Y' to use.
	YCbCrLumaCoefficients luma_coefficients;

	// If true, assume Y'CbCr coefficients are full-range, ie. go from 0 to 255
	// instead of the limited 220/225 steps in classic MPEG. For instance,
	// JPEG uses the Rec. 601 luma coefficients, but full range.
	bool full_range;

	// Sampling factors for chroma components. For no subsampling (4:4:4),
	// set both to 1.
	unsigned chroma_subsampling_x, chroma_subsampling_y;

	// Positioning of the chroma samples. MPEG-1 and JPEG is (0.5, 0.5);
	// MPEG-2 and newer typically are (0.0, 0.5).
	float cb_x_position, cb_y_position;
	float cr_x_position, cr_y_position;
};

// Convert texel sampling offset for the given chroma channel, given that
// chroma position is <pos> (0..1), we are downsampling this chroma channel
// by a factor of <subsampling_factor> and the texture we are sampling from
// is <resolution> pixels wide/high.
float compute_chroma_offset(float pos, unsigned subsampling_factor, unsigned resolution);

// Given <ycbcr_format>, compute the values needed to turn Y'CbCr into R'G'B';
// first subtract the returned offset, then left-multiply the returned matrix
// (the scaling is already folded into it).
void compute_ycbcr_matrix(YCbCrFormat ycbcr_format, float *offset, Eigen::Matrix3d *ycbcr_to_rgb);

}  // namespace movit

#endif // !defined(_MOVIT_YCBCR_INPUT_H)
