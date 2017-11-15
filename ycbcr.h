#ifndef _MOVIT_YCBCR_H
#define _MOVIT_YCBCR_H 1

// Shared utility functions between YCbCrInput, YCbCr422InterleavedInput
// and YCbCrConversionEffect.
//
// Conversion from integer to floating-point representation in case of
// Y'CbCr is seemingly tricky:
//
// BT.601 page 8 has a table that says that for luma, black is at 16.00_d and
// white is at 235.00_d. _d seemingly means “on a floating-point scale from 0
// to 255.75”, see §2.4. The .75 is because BT.601 wants to support 10-bit,
// but all values are scaled for 8-bit since that's the most common; it is
// specified that conversion from 8-bit to 10-bit is done by inserting two
// binary zeroes at the end (not repeating bits as one would often do
// otherwise). It would seem that BT.601 lives in a world where the idealized
// range is really [0,256), not [0,255].
//
// However, GPUs (and by extension Movit) don't work this way. For them,
// typically 1.0 maps to the largest possible representable value in the
// framebuffer, ie., the range [0.0,1.0] maps to [0,255] for 8-bit
// and to [0,1023] (or [0_d,255.75_d] in BT.601 parlance) for 10-bit.
//
// BT.709 (page 5) seems to agree with BT.601; it specifies range 16–235 for
// 8-bit luma, and 64–940 for 10-bit luma. This would indicate, for a GPU,
// that that for 8-bit mode, the range would be 16/255 to 235/255
// (0.06275 to 0.92157), while for 10-bit, it should be 64/1023 to 940/1023
// (0.06256 to 0.91887). There's no good compromise here; if you select 8-bit
// range, 10-bit goes out of range (white gets to 942), while if you select
// 10-bit range, 8-bit gets only to 234, making true white impossible.
//
// Thus, you will need to specify the actual precision of the Y'CbCr source
// (or destination); the num_levels field is the right place. Most people
// will want to simply set this to 256, as 8-bit Y'CbCr is the most common,
// but the right value will naturally depend on your input.
//
// We could use unsigned formats (e.g. GL_R8UI), which in a sense would
// solve all of this, but then we'd lose filtering.

#include "image_format.h"

#include <epoxy/gl.h>
#include <Eigen/Core>

namespace movit {

struct YCbCrFormat {
	// Which formula for Y' to use.
	YCbCrLumaCoefficients luma_coefficients;

	// If true, assume Y'CbCr coefficients are full-range, ie. go from 0 to 255
	// instead of the limited 220/225 steps in classic MPEG. For instance,
	// JPEG uses the Rec. 601 luma coefficients, but full range.
	bool full_range;

	// Set to 2^n for n-bit Y'CbCr (e.g. 256 for 8-bit Y'CbCr).
	// See file-level comment.
	int num_levels;

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
//
// <type> is the data type you're rendering from; normally, it would should match
// <ycbcr_format.num_levels>, but for the special case of 10- and 12-bit Y'CbCr,
// we support storing it in 16-bit formats, which incurs extra scaling factors.
// You can get that scaling factor in <scale> if you want.
void compute_ycbcr_matrix(YCbCrFormat ycbcr_format, float *offset, Eigen::Matrix3d *ycbcr_to_rgb,
                          GLenum type = GL_UNSIGNED_BYTE, double *scale_factor = nullptr);

}  // namespace movit

#endif // !defined(_MOVIT_YCBCR_INPUT_H)
