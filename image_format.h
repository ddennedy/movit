#ifndef _MOVIT_IMAGE_FORMAT_H
#define _MOVIT_IMAGE_FORMAT_H 1

// Note: Input depths above 8 bits have not been tested, so Rec. 2020
// support should be regarded as somewhat untested (it assumes 10-
// or 12-bit input). We also only support “conventional non-constant
// luminance” for Rec. 2020, where Y' is derived from R'G'B' instead of
// RGB, since this is the same system as used in Rec. 601 and 709.

enum MovitPixelFormat {
	FORMAT_RGB,
	FORMAT_RGBA_PREMULTIPLIED_ALPHA,
	FORMAT_RGBA_POSTMULTIPLIED_ALPHA,
	FORMAT_BGR,
	FORMAT_BGRA_PREMULTIPLIED_ALPHA,
	FORMAT_BGRA_POSTMULTIPLIED_ALPHA,
	FORMAT_GRAYSCALE
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

#endif  // !defined(_MOVIT_IMAGE_FORMAT_H)
