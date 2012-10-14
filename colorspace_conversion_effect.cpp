#include <assert.h>

#include "colorspace_conversion_effect.h"
#include "util.h"

// Color coordinates from Rec. 709; sRGB uses the same primaries.
double rec709_x_R = 0.640,  rec709_x_G = 0.300,  rec709_x_B = 0.150;
double rec709_y_R = 0.330,  rec709_y_G = 0.600,  rec709_y_B = 0.060;

// Color coordinates from Rec. 601. (Separate for 525- and 625-line systems.)
double rec601_525_x_R = 0.630, rec601_525_x_G = 0.310, rec601_525_x_B = 0.155;
double rec601_525_y_R = 0.340, rec601_525_y_G = 0.595, rec601_525_y_B = 0.070;
double rec601_625_x_R = 0.640, rec601_625_x_G = 0.290, rec601_625_x_B = 0.150;
double rec601_625_y_R = 0.330, rec601_625_y_G = 0.600, rec601_625_y_B = 0.060;

// The D65 white point. Given in both Rec. 601 and 709.
double d65_x = 0.3127, d65_y = 0.3290;

ColorSpaceConversionEffect::ColorSpaceConversionEffect()
	: source_space(COLORSPACE_sRGB),
	  destination_space(COLORSPACE_sRGB)
{
	register_int("source_space", (int *)&source_space);
	register_int("destination_space", (int *)&destination_space);
}

void get_xyz_matrix(ColorSpace space, Matrix3x3 m)
{
	if (space == COLORSPACE_XYZ) {
		m[0] = 1.0f; m[3] = 0.0f; m[6] = 0.0f;
		m[1] = 0.0f; m[4] = 1.0f; m[7] = 0.0f;
		m[2] = 0.0f; m[5] = 0.0f; m[8] = 1.0f;
		return;
	}

	double x_R, x_G, x_B;
	double y_R, y_G, y_B;

	switch (space) {
	case COLORSPACE_REC_709:  // And sRGB.
		x_R = rec709_x_R; x_G = rec709_x_G; x_B = rec709_x_B;
		y_R = rec709_y_R; y_G = rec709_y_G; y_B = rec709_y_B;
		break;
	case COLORSPACE_REC_601_525:
		x_R = rec601_525_x_R; x_G = rec601_525_x_G; x_B = rec601_525_x_B;
		y_R = rec601_525_y_R; y_G = rec601_525_y_G; y_B = rec601_525_y_B;
		break;
	case COLORSPACE_REC_601_625:
		x_R = rec601_625_x_R; x_G = rec601_625_x_G; x_B = rec601_625_x_B;
		y_R = rec601_625_y_R; y_G = rec601_625_y_G; y_B = rec601_625_y_B;
		break;
	default:
		assert(false);
	}

	// Recover z = 1 - x - y.
	double z_R = 1.0 - x_R - y_R;
	double z_G = 1.0 - x_G - y_G;
	double z_B = 1.0 - x_B - y_B;

	// Find the XYZ coordinates of D65 (white point for both Rec. 601 and 709),
	// normalized so that Y=1.
	double d65_X = d65_x / d65_y;
	double d65_Y = 1.0;
	double d65_Z = (1.0 - d65_x - d65_y) / d65_y;

	// We have, for each primary (example is with red):
	//
	//   X_R / (X_R + Y_R + Z_R) = x_R
	//   Y_R / (X_R + Y_R + Z_R) = y_R
	//   Z_R / (X_R + Y_R + Z_R) = z_R
	//
	// Some algebraic fiddling yields (unsurprisingly):
	//
	//   X_R = (x_R / y_R) Y_R
	//   Z_R = (z_R / y_R) Z_R
	//
	// We also know that since RGB=(1,1,1) should give us the
	// D65 illuminant, we must have
	//
	//   X_R + X_G + X_B = D65_X
	//   Y_R + Y_G + Y_B = D65_Y
	//   Z_R + Z_G + Z_B = D65_Z
	//
	// But since we already know how to express Y and Z by
	// some constant multiple of X, this reduces to
	//
	//   k1 Y_R + k2 Y_G + k3 Y_B = D65_X
	//      Y_R +    Y_G +    Y_B = D65_Y
	//   k4 Y_R + k5 Y_G + k6 Y_B = D65_Z
	//
	// Which we can solve for (Y_R, Y_G, Y_B) by inverting a 3x3 matrix.

	Matrix3x3 temp, inverted;
	temp[0] = x_R / y_R;
	temp[3] = x_G / y_G;
	temp[6] = x_B / y_B;

	temp[1] = 1.0;
	temp[4] = 1.0;
	temp[7] = 1.0;

	temp[2] = z_R / y_R;
	temp[5] = z_G / y_G;
	temp[8] = z_B / y_B;

	invert_3x3_matrix(temp, inverted);
	float Y_R, Y_G, Y_B;
	multiply_3x3_matrix_float3(inverted, d65_X, d65_Y, d65_Z, &Y_R, &Y_G, &Y_B);

	// Now convert xyY -> XYZ.
	double X_R = temp[0] * Y_R;
	double Z_R = temp[2] * Y_R;
	double X_G = temp[3] * Y_G;
	double Z_G = temp[5] * Y_G;
	double X_B = temp[6] * Y_B;
	double Z_B = temp[8] * Y_B;

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
		m[0], m[1], m[2],
		m[3], m[4], m[5],
		m[6], m[7], m[8]);
	return buf + read_file("colorspace_conversion_effect.frag");
}
