#ifndef _UTIL_H
#define _UTIL_H 1

// assumes h in [0, 2pi> or [-pi, pi>
void hsv2rgb(float h, float s, float v, float *r, float *g, float *b);

#endif // !defined(_UTIL_H)
