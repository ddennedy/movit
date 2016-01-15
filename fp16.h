#ifndef _MOVIT_FP16_H
#define _MOVIT_FP16_H 1

#ifdef __F16C__
#include <immintrin.h>
#endif

// Code for converting to and from fp16 (from fp64), without any particular
// machine support, with proper IEEE round-to-even behavior (and correct
// handling of NaNs and infinities). This is needed because some OpenGL
// drivers don't properly round off when asked to convert data themselves.
//
// These routines are not particularly fast.

namespace movit {

// structs instead of ints, so that they are not implicitly convertible.
struct fp32_int_t {
	unsigned int val;
};
struct fp16_int_t {
	unsigned short val;
};

#ifdef __F16C__

// Use the f16c instructions from Haswell if available (and we know that they
// are at compile time).
static inline float fp16_to_fp32(fp16_int_t x)
{
	return _cvtsh_ss(x.val);
}

static inline fp16_int_t fp32_to_fp16(float x)
{
	fp16_int_t ret;
	ret.val = _cvtss_sh(x, 0);
	return ret;
}

#else

float fp16_to_fp32(fp16_int_t x);
fp16_int_t fp32_to_fp16(float x);

#endif

// Overloads for use in templates.
static inline float to_fp32(double x) { return x; }
static inline float to_fp32(float x) { return x; }
static inline float to_fp32(fp16_int_t x) { return fp16_to_fp32(x); }

template<class T> inline T from_fp32(float x);
template<> inline double from_fp32<double>(float x) { return x; }
template<> inline float from_fp32<float>(float x) { return x; }
template<> inline fp16_int_t from_fp32<fp16_int_t>(float x) { return fp32_to_fp16(x); }

template<class From, class To>
inline To convert_float(From x) { return from_fp32<To>(to_fp32(x)); }

template<class Same>
inline Same convert_float(Same x) { return x; }

}  // namespace movit

#endif  // _MOVIT_FP16_H
