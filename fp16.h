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
// These routines are originally written by Fabian Giesen, and released by
// him into the public domain;
// see https://fgiesen.wordpress.com/2012/03/28/half-to-float-done-quic/.
// They are quite fast, and can be vectorized if need be; of course, using
// the f16c instructions (see below) will be faster still.

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

union fp32 {
	float f;
	unsigned int u;
};

static inline float fp16_to_fp32(fp16_int_t h)
{
	fp32 magic; magic.u = 113 << 23;
	unsigned int shifted_exp = 0x7c00 << 13;  // exponent mask after shift
	fp32 o;

	// mantissa+exponent
	unsigned int shifted = (h.val & 0x7fff) << 13;
	unsigned int exponent = shifted & shifted_exp;

	// exponent cases
	o.u = shifted;
	if (exponent == 0) {  // Zero / Denormal
		o.u += magic.u;
		o.f -= magic.f;
	} else if (exponent == shifted_exp) {  // Inf/NaN
		o.u += (255 - 31) << 23;
	} else {
		o.u += (127 - 15) << 23;
	}

	o.u |= (h.val & 0x8000) << 16;  // copy sign bit
	return o.f;
}

static inline fp16_int_t fp32_to_fp16(float x)
{
	fp32 f; f.f = x;
	unsigned int f32infty = 255 << 23;
	unsigned int f16max = (127 + 16) << 23;
	fp32 denorm_magic; denorm_magic.u = ((127 - 15) + (23 - 10) + 1) << 23;
	unsigned int sign_mask = 0x80000000u;
	fp16_int_t o = { 0 };

	unsigned int sign = f.u & sign_mask;
	f.u ^= sign;

	// NOTE all the integer compares in this function can be safely
	// compiled into signed compares since all operands are below
	// 0x80000000. Important if you want fast straight SSE2 code
	// (since there's no unsigned PCMPGTD).

	if (f.u >= f16max) {  // result is Inf or NaN (all exponent bits set)
		o.val = (f.u > f32infty) ? 0x7e00 : 0x7c00; // NaN->qNaN and Inf->Inf
	} else {  // (De)normalized number or zero
		if (f.u < (113 << 23)) {  // resulting FP16 is subnormal or zero
			// use a magic value to align our 10 mantissa bits at the bottom of
			// the float. as long as FP addition is round-to-nearest-even this
			// just works.
			f.f += denorm_magic.f;

			// and one integer subtract of the bias later, we have our final float!
			o.val = f.u - denorm_magic.u;
		} else {
			unsigned int mant_odd = (f.u >> 13) & 1; // resulting mantissa is odd

			// update exponent, rounding bias part 1
			f.u += (unsigned(15 - 127) << 23) + 0xfff;
			// rounding bias part 2
			f.u += mant_odd;
			// take the bits!
			o.val = f.u >> 13;
		}
	}

	o.val |= sign >> 16;
	return o;
}

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
