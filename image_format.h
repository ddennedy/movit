#ifndef _MOVIT_IMAGE_FORMAT_H
#define _MOVIT_IMAGE_FORMAT_H 1

// Note: Movit's internal processing formats do not have enough
// accuracy to support 12-bit input, so if you want to use Rec. 2020,
// you should probably stick to 10-bit, or accept somewhat reduced
// accuracy for 12-bit. Input depths above 8 bits are also generally
// less tested.
//
// We also only support “conventional non-constant luminance” for Rec. 2020,
// where Y' is derived from R'G'B' instead of RGB, since this is the same
// system as used in Rec. 601 and 709.

namespace movit {

enum MovitPixelFormat {
	FORMAT_RGB,
	FORMAT_RGBA_PREMULTIPLIED_ALPHA,
	FORMAT_RGBA_POSTMULTIPLIED_ALPHA,
	FORMAT_BGR,
	FORMAT_BGRA_PREMULTIPLIED_ALPHA,
	FORMAT_BGRA_POSTMULTIPLIED_ALPHA,
	FORMAT_GRAYSCALE,
	FORMAT_RG,
	FORMAT_R
};

enum Colorspace {
	COLORSPACE_INVALID = -1,  // For internal use.
	COLORSPACE_sRGB = 0,
	COLORSPACE_REC_709 = 0,  // Same as sRGB.
	COLORSPACE_REC_601_525 = 1,
	COLORSPACE_REC_601_625 = 2,
	COLORSPACE_XYZ = 3,  // Mostly useful for testing and debugging.
	COLORSPACE_REC_2020 = 4,
};

enum GammaCurve {
	GAMMA_INVALID = -1,  // For internal use.
	GAMMA_LINEAR = 0,
	GAMMA_sRGB = 1,
	GAMMA_REC_601 = 2,
	GAMMA_REC_709 = 2,  // Same as Rec. 601.
	GAMMA_REC_2020_10_BIT = 2,  // Same as Rec. 601.
	GAMMA_REC_2020_12_BIT = 3,
};

enum YCbCrLumaCoefficients {
	YCBCR_REC_601 = 0,
	YCBCR_REC_709 = 1,
	YCBCR_REC_2020 = 2,
};

struct ImageFormat {
	Colorspace color_space;
	GammaCurve gamma_curve;
};

}  // namespace movit

#endif  // !defined(_MOVIT_IMAGE_FORMAT_H)
