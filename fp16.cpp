#include "fp16.h"

namespace movit {
namespace {

union fp32 {
	float f;
	unsigned int u;
};

}  // namespace

#ifndef __F16C__

float fp16_to_fp32(fp16_int_t h)
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

fp16_int_t fp32_to_fp16(float x)
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
			f.u += ((15 - 127) << 23) + 0xfff;
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

}  // namespace
