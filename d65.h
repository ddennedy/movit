#ifndef _D65_H
#define _D65_H 1

// The D65 illuminant, which is the standard white point (ie. what you should get
// for R=G=B=1) for almost all video color spaces in common use. It has a color
// temperature roughly around 6500 K, which is sort of bluish; it is intended to
// simulate average daylight conditions.
//
// The definition (in xyz space) is given, for instance, in both Rec. 601 and 709.
static const double d65_x = 0.3127, d65_y = 0.3290, d65_z = 1.0 - d65_x - d65_y;

#endif  // !defined(_D65_H)

