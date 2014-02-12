#include <assert.h>
#include <Eigen/Core>
#include <Eigen/LU>

#include "colorspace_conversion_effect.h"
#include "d65.h"
#include "util.h"

using namespace Eigen;
using namespace std;

namespace movit {

// Color coordinates from Rec. 709; sRGB uses the same primaries.
static const double rec709_x_R = 0.640, rec709_x_G = 0.300, rec709_x_B = 0.150;
static const double rec709_y_R = 0.330, rec709_y_G = 0.600, rec709_y_B = 0.060;

// Color coordinates from Rec. 601. (Separate for 525- and 625-line systems.)
static const double rec601_525_x_R = 0.630, rec601_525_x_G = 0.310, rec601_525_x_B = 0.155;
static const double rec601_525_y_R = 0.340, rec601_525_y_G = 0.595, rec601_525_y_B = 0.070;
static const double rec601_625_x_R = 0.640, rec601_625_x_G = 0.290, rec601_625_x_B = 0.150;
static const double rec601_625_y_R = 0.330, rec601_625_y_G = 0.600, rec601_625_y_B = 0.060;

// Color coordinates from Rec. 2020.
static const double rec2020_x_R = 0.708, rec2020_x_G = 0.170, rec2020_x_B = 0.131;
static const double rec2020_y_R = 0.292, rec2020_y_G = 0.797, rec2020_y_B = 0.046;

ColorspaceConversionEffect::ColorspaceConversionEffect()
	: source_space(COLORSPACE_sRGB),
	  destination_space(COLORSPACE_sRGB)
{
	register_int("source_space", (int *)&source_space);
	register_int("destination_space", (int *)&destination_space);
}

Matrix3d ColorspaceConversionEffect::get_xyz_matrix(Colorspace space)
{
	if (space == COLORSPACE_XYZ) {
		return Matrix3d::Identity();
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
	case COLORSPACE_REC_2020:
		x_R = rec2020_x_R; x_G = rec2020_x_G; x_B = rec2020_x_B;
		y_R = rec2020_y_R; y_G = rec2020_y_G; y_B = rec2020_y_B;
		break;
	default:
		assert(false);
	}

	// Recover z = 1 - x - y.
	double z_R = 1.0 - x_R - y_R;
	double z_G = 1.0 - x_G - y_G;
	double z_B = 1.0 - x_B - y_B;

	// We have, for each primary (example is with red):
	//
	//   X_R / (X_R + Y_R + Z_R) = x_R
	//   Y_R / (X_R + Y_R + Z_R) = y_R
	//   Z_R / (X_R + Y_R + Z_R) = z_R
	//
	// Some algebraic fiddling yields (unsurprisingly):
	//
	//   X_R = (x_R / y_R) Y_R   (so define k1 = x_R / y_R)
	//   Z_R = (z_R / y_R) Y_R   (so define k4 = z_R / y_R)
	//
	// We also know that since RGB=(1,1,1) should give us the
	// D65 illuminant, we must have
	//
	//   X_R + X_G + X_B = D65_X
	//   Y_R + Y_G + Y_B = D65_Y
	//   Z_R + Z_G + Z_B = D65_Z
	//
	// But since we already know how to express X and Z by
	// some constant multiple of Y, this reduces to
	//
	//   k1 Y_R + k2 Y_G + k3 Y_B = D65_X
	//      Y_R +    Y_G +    Y_B = D65_Y
	//   k4 Y_R + k5 Y_G + k6 Y_B = D65_Z
	//
	// Which we can solve for (Y_R, Y_G, Y_B) by inverting a 3x3 matrix.

	Matrix3d temp;
	temp(0,0) = x_R / y_R;
	temp(0,1) = x_G / y_G;
	temp(0,2) = x_B / y_B;

	temp(1,0) = 1.0;
	temp(1,1) = 1.0;
	temp(1,2) = 1.0;

	temp(2,0) = z_R / y_R;
	temp(2,1) = z_G / y_G;
	temp(2,2) = z_B / y_B;

	Vector3d d65_XYZ(d65_X, d65_Y, d65_Z);
	Vector3d Y_RGB = temp.inverse() * d65_XYZ;

	// Now convert xyY -> XYZ.
	double X_R = temp(0,0) * Y_RGB[0];
	double Z_R = temp(2,0) * Y_RGB[0];

	double X_G = temp(0,1) * Y_RGB[1];
	double Z_G = temp(2,1) * Y_RGB[1];

	double X_B = temp(0,2) * Y_RGB[2];
	double Z_B = temp(2,2) * Y_RGB[2];

	Matrix3d m;
	m(0,0) = X_R;      m(0,1) = X_G;      m(0,2) = X_B;
	m(1,0) = Y_RGB[0]; m(1,1) = Y_RGB[1]; m(1,2) = Y_RGB[2];
	m(2,0) = Z_R;      m(2,1) = Z_G;      m(2,2) = Z_B;

	return m;
}

string ColorspaceConversionEffect::output_fragment_shader()
{
	// Create a matrix to convert from source space -> XYZ,
	// another matrix to convert from XYZ -> destination space,
	// and then concatenate the two.
	//
	// Since we right-multiply the RGB column vector, the matrix
	// concatenation order needs to be the opposite of the operation order.
	Matrix3d source_space_to_xyz = get_xyz_matrix(source_space);
	Matrix3d xyz_to_destination_space = get_xyz_matrix(destination_space).inverse();
	Matrix3d m = xyz_to_destination_space * source_space_to_xyz;

	return output_glsl_mat3("PREFIX(conversion_matrix)", m) +
		read_file("colorspace_conversion_effect.frag");
}

}  // namespace movit
