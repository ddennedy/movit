#ifndef _IMAGE_FORMAT_H
#define _IMAGE_FORMAT_H 1

enum MovitPixelFormat { FORMAT_RGB, FORMAT_RGBA, FORMAT_BGR, FORMAT_BGRA, FORMAT_GRAYSCALE };

enum Colorspace {
	COLORSPACE_INVALID = -1,  // For internal use.
	COLORSPACE_sRGB = 0,
	COLORSPACE_REC_709 = 0,  // Same as sRGB.
	COLORSPACE_REC_601_525 = 1,
	COLORSPACE_REC_601_625 = 2,
	COLORSPACE_XYZ = 3,  // Mostly useful for testing and debugging.
};

enum GammaCurve {
	GAMMA_INVALID = -1,  // For internal use.
	GAMMA_LINEAR = 0,
	GAMMA_sRGB = 1,
	GAMMA_REC_601 = 2,
	GAMMA_REC_709 = 2,  // Same as Rec. 601.
};

enum YCbCrLumaCoefficients {
	YCBCR_REC_601 = 0,
	YCBCR_REC_709 = 1,
};

struct ImageFormat {
	Colorspace color_space;
	GammaCurve gamma_curve;
};

#endif  // !defined(_IMAGE_FORMAT_H)
