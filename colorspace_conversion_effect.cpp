#include <assert.h>

#include "colorspace_conversion_effect.h"
#include "util.h"

// Color coordinates from Rec. 709; sRGB uses the same primaries.
double rec709_x_R = 0.640,  rec709_x_G = 0.300,  rec709_x_B = 0.150;
double rec709_y_R = 0.330,  rec709_y_G = 0.600,  rec709_y_B = 0.060;
double rec709_Y_R = 0.2126, rec709_Y_G = 0.7152, rec709_Y_B = 0.0722;

// Color coordinates from Rec. 601. (Separate for 525- and 625-line systems.)
double rec601_525_x_R = 0.630, rec601_525_x_G = 0.310, rec601_525_x_B = 0.155;
double rec601_525_y_R = 0.340, rec601_525_y_G = 0.595, rec601_525_y_B = 0.070;
double rec601_625_x_R = 0.640, rec601_625_x_G = 0.290, rec601_625_x_B = 0.150;
double rec601_625_y_R = 0.330, rec601_625_y_G = 0.600, rec601_625_y_B = 0.060;
double rec601_Y_R = 0.299, rec601_Y_G = 0.587, rec601_Y_B = 0.114;

ColorSpaceConversionEffect::ColorSpaceConversionEffect()
	: source_space(COLORSPACE_sRGB),
	  destination_space(COLORSPACE_sRGB)
{
	register_int("source_space", (int *)&source_space);
	register_int("destination_space", (int *)&destination_space);
}

void get_xyz_matrix(ColorSpace space, Matrix3x3 m)
{
	double x_R, x_G, x_B;
	double y_R, y_G, y_B;
	double Y_R, Y_G, Y_B;

	switch (space) {
	case COLORSPACE_REC_709:  // And sRGB.
		x_R = rec709_x_R; x_G = rec709_x_G; x_B = rec709_x_B;
		y_R = rec709_y_R; y_G = rec709_y_G; y_B = rec709_y_B;
		Y_R = rec709_Y_R; Y_G = rec709_Y_G; Y_B = rec709_Y_B;
		break;
	case COLORSPACE_REC_601_525:
		x_R = rec601_525_x_R; x_G = rec601_525_x_G; x_B = rec601_525_x_B;
		y_R = rec601_525_y_R; y_G = rec601_525_y_G; y_B = rec601_525_y_B;
		Y_R = rec601_Y_R;     Y_G = rec601_Y_G;     Y_B = rec601_Y_B;
		break;
	case COLORSPACE_REC_601_625:
		x_R = rec601_625_x_R; x_G = rec601_625_x_G; x_B = rec601_625_x_B;
		y_R = rec601_625_y_R; y_G = rec601_625_y_G; y_B = rec601_625_y_B;
		Y_R = rec601_Y_R;     Y_G = rec601_Y_G;     Y_B = rec601_Y_B;
		break;
	default:
		assert(false);
	}

	// Convert xyY -> XYZ.
	double X_R, X_G, X_B;
	X_R = Y_R * x_R / y_R;
	X_G = Y_G * x_G / y_G;
	X_B = Y_B * x_B / y_B;
	
	double Z_R, Z_G, Z_B;
	Z_R = Y_R * (1.0f - x_R - y_R) / y_R;
	Z_G = Y_G * (1.0f - x_G - y_G) / y_G;
	Z_B = Y_B * (1.0f - x_B - y_B) / y_B;

	m[0] = X_R; m[3] = X_G; m[6] = X_B;
	m[1] = Y_R; m[4] = Y_G; m[7] = Y_B;
	m[2] = Z_R; m[5] = Z_G; m[8] = Z_B;
}

std::string ColorSpaceConversionEffect::output_fragment_shader()
{
	// Create a matrix to convert from source space -> XYZ,
	// another matrix to convert from XYZ -> destination space,
	// and then concatenate the two.
	//
	// Since we right-multiply the RGB column vector, the matrix
	// concatenation order needs to be the opposite of the operation order.
	Matrix3x3 m;

	Matrix3x3 source_space_to_xyz;
	Matrix3x3 destination_space_to_xyz;
	Matrix3x3 xyz_to_destination_space;

	get_xyz_matrix(source_space, source_space_to_xyz);
	get_xyz_matrix(destination_space, destination_space_to_xyz);
	invert_3x3_matrix(destination_space_to_xyz, xyz_to_destination_space);
	
	multiply_3x3_matrices(xyz_to_destination_space, source_space_to_xyz, m);

	char buf[1024];
	sprintf(buf,
		"const mat3 PREFIX(conversion_matrix) = mat3(\n"
		"    %.8f, %.8f, %.8f,\n"
		"    %.8f, %.8f, %.8f,\n"
		"    %.8f, %.8f, %.8f);\n\n",
		m[0], m[3], m[6],
		m[1], m[4], m[7],
		m[2], m[5], m[8]);
	return buf + read_file("colorspace_conversion_effect.frag");
}
