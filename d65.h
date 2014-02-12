#ifndef _MOVIT_D65_H
#define _MOVIT_D65_H 1

namespace movit {

// The D65 illuminant, which is the standard white point (ie. what you should get
// for R=G=B=1) for almost all video color spaces in common use. It has a color
// temperature roughly around 6500 K, which is sort of bluish; it is intended to
// simulate average daylight conditions.
//
// The definition (in xyz space) is given, for instance, in both Rec. 601 and 709.
static const double d65_x = 0.3127, d65_y = 0.3290, d65_z = 1.0 - d65_x - d65_y;

// XYZ coordinates of D65, normalized so that Y=1.
static const double d65_X = d65_x / d65_y;
static const double d65_Y = 1.0;
static const double d65_Z = d65_z / d65_y;

}  // namespace movit

#endif  // !defined(_MOVIT_D65_H)

