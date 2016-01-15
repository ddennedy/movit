#include "fp16.h"

namespace movit {
namespace {

union fp32 {
	float f;
	unsigned int u;
};

template<class FP16_INT_T,
         int FP16_BIAS, int FP16_MANTISSA_BITS, int FP16_EXPONENT_BITS, int FP16_MAX_EXPONENT,
         int FP32_BIAS, int FP32_MANTISSA_BITS, int FP32_EXPONENT_BITS, int FP32_MAX_EXPONENT>
inline float fp_upconvert(FP16_INT_T x)
{
	int sign = x.val >> (FP16_MANTISSA_BITS + FP16_EXPONENT_BITS);
	int exponent = (x.val & ((1U << (FP16_MANTISSA_BITS + FP16_EXPONENT_BITS)) - 1)) >> FP16_MANTISSA_BITS;
	unsigned int mantissa = x.val & ((1U << FP16_MANTISSA_BITS) - 1);

	int sign32;
	int exponent32;
	unsigned int mantissa32;

	if (exponent == 0) {
		/* 
		 * Denormals, or zero. Zero is still zero, denormals become
		 * ordinary numbers.
		 */
		if (mantissa == 0) {
			sign32 = sign;
			exponent32 = 0;
			mantissa32 = 0;
		} else {
			sign32 = sign;
			exponent32 = FP32_BIAS - FP16_BIAS;
			mantissa32 = mantissa << (FP32_MANTISSA_BITS - FP16_MANTISSA_BITS + 1);

			/* Normalize the number. */
			while ((mantissa32 & (1U << FP32_MANTISSA_BITS)) == 0) {
				--exponent32;
				mantissa32 <<= 1;
			}

			/* Clear the now-implicit one-bit. */
			mantissa32 &= ~(1U << FP32_MANTISSA_BITS);
		}
	} else if (exponent == FP16_MAX_EXPONENT) {
		/*
		 * Infinities or NaN (mantissa=0 => infinity, otherwise NaN).
		 * We don't care much about NaNs, so let us just make sure we
		 * keep the first bit (which signals signalling/non-signalling
		 * in many implementations).
		 */
		sign32 = sign;
		exponent32 = FP32_MAX_EXPONENT;
		mantissa32 = mantissa << (FP32_MANTISSA_BITS - FP16_MANTISSA_BITS);
	} else {
		sign32 = sign;

		/* Up-conversion is simple. Just re-bias the exponent... */
		exponent32 = exponent + FP32_BIAS - FP16_BIAS;

		/* ...and convert the mantissa. */
		mantissa32 = mantissa << (FP32_MANTISSA_BITS - FP16_MANTISSA_BITS);
	}

	union fp32 nx;
	nx.u = ((unsigned int)sign32 << (FP32_MANTISSA_BITS + FP32_EXPONENT_BITS))
	    | ((unsigned int)exponent32 << FP32_MANTISSA_BITS)
	    | mantissa32;
	return nx.f;
}

unsigned int shift_right_with_round(unsigned int x, unsigned shift)
{
	/* shifts >= 32 need to be special-cased */
	if (shift > 32) {
		return 0;
	} else if (shift == 32) {
		if (x > (1U << 31)) {
			return 1;
		} else {
			return 0;
		}
	}

	unsigned int round_part = x & ((1U << shift) - 1);
	if (round_part < (1U << (shift - 1))) {
		/* round down */
		x >>= shift;
	} else if (round_part > (1U << (shift - 1))) {
		/* round up */
		x >>= shift;
		++x;
	} else {
		/* round to nearest even number */
		x >>= shift;
		if ((x & 1) != 0) {
			++x;
		}
	}
	return x;
}

template<class FP16_INT_T,
         int FP16_BIAS, int FP16_MANTISSA_BITS, int FP16_EXPONENT_BITS, int FP16_MAX_EXPONENT,
         int FP32_BIAS, int FP32_MANTISSA_BITS, int FP32_EXPONENT_BITS, int FP32_MAX_EXPONENT>
inline FP16_INT_T fp_downconvert(float x)
{
	union fp32 nx;
	nx.f = x;
	unsigned int f = nx.u;
	int sign = f >> (FP32_MANTISSA_BITS + FP32_EXPONENT_BITS);
	int exponent = (f & ((1U << (FP32_MANTISSA_BITS + FP32_EXPONENT_BITS)) - 1)) >> FP32_MANTISSA_BITS;
	unsigned int mantissa = f & ((1U << FP32_MANTISSA_BITS) - 1);

	int sign16;
	int exponent16;
	unsigned int mantissa16;

	if (exponent == 0) {
		/*
		 * Denormals, or zero. The largest possible 32-bit
		 * denormal is about +- 2^-1022, and the smallest possible
		 * 16-bit denormal is +- 2^-24. Thus, we can safely
		 * just set all of these to zero (but keep the sign bit).
		 */
		sign16 = sign;
		exponent16 = 0;
		mantissa16 = 0;
	} else if (exponent == FP32_MAX_EXPONENT) {
		/*
		 * Infinities or NaN (mantissa=0 => infinity, otherwise NaN).
		 * We don't care much about NaNs, so let us just keep the first
		 * bit (which signals signalling/ non-signalling) and make sure 
		 * that we don't coerce NaNs down to infinities.
		 */
		if (mantissa == 0) {
			sign16 = sign;
			exponent16 = FP16_MAX_EXPONENT;
			mantissa16 = 0;
		} else {
			sign16 = sign;  /* undefined */
			exponent16 = FP16_MAX_EXPONENT;
			mantissa16 = mantissa >> (FP32_MANTISSA_BITS - FP16_MANTISSA_BITS);
			if (mantissa16 == 0) {
				mantissa16 = 1;
			}
		}
	} else {
		/* Re-bias the exponent, and check if we will create a denormal. */
		exponent16 = exponent + FP16_BIAS - FP32_BIAS;
		if (exponent16 <= 0) {
			int shift_amount = FP32_MANTISSA_BITS - FP16_MANTISSA_BITS - exponent16 + 1;
			sign16 = sign;
			exponent16 = 0;
			mantissa16 = shift_right_with_round(mantissa | (1U << FP32_MANTISSA_BITS), shift_amount);

			/*
			 * We could actually have rounded back into the lowest possible non-denormal
			 * here, so check for that.
			 */
			if (mantissa16 == (1U << FP16_MANTISSA_BITS)) {
				exponent16 = 1;
				mantissa16 = 0;
			}
		} else {
			/*
			 * First, round off the mantissa, since that could change
			 * the exponent. We use standard IEEE 754r roundTiesToEven
			 * mode.
			 */
			sign16 = sign;
			mantissa16 = shift_right_with_round(mantissa, FP32_MANTISSA_BITS - FP16_MANTISSA_BITS);

			/* Check if we overflowed and need to increase the exponent. */
			if (mantissa16 == (1U << FP16_MANTISSA_BITS)) {
				++exponent16;
				mantissa16 = 0;
			}

			/* Finally, check for overflow, and create +- inf if we need to. */
			if (exponent16 >= FP16_MAX_EXPONENT) {
				exponent16 = FP16_MAX_EXPONENT;
				mantissa16 = 0;
			}
		}
	}

	FP16_INT_T ret;
	ret.val = (sign16 << (FP16_MANTISSA_BITS + FP16_EXPONENT_BITS))
	    | (exponent16 << FP16_MANTISSA_BITS)
	    | mantissa16;
	return ret;
}

const int FP64_BIAS = 1023;
const int FP64_MANTISSA_BITS = 52;
const int FP64_EXPONENT_BITS = 11;
const int FP64_MAX_EXPONENT = (1 << FP64_EXPONENT_BITS) - 1;

const int FP32_BIAS = 127;
const int FP32_MANTISSA_BITS = 23;
const int FP32_EXPONENT_BITS = 8;
const int FP32_MAX_EXPONENT = (1 << FP32_EXPONENT_BITS) - 1;

const int FP16_BIAS = 15;
const int FP16_MANTISSA_BITS = 10;
const int FP16_EXPONENT_BITS = 5;
const int FP16_MAX_EXPONENT = (1 << FP16_EXPONENT_BITS) - 1;

}  // namespace

#ifndef __F16C__

float fp16_to_fp32(fp16_int_t x)
{
	return fp_upconvert<fp16_int_t,
	       FP16_BIAS, FP16_MANTISSA_BITS, FP16_EXPONENT_BITS, FP16_MAX_EXPONENT,
	       FP32_BIAS, FP32_MANTISSA_BITS, FP32_EXPONENT_BITS, FP32_MAX_EXPONENT>(x);
}

fp16_int_t fp32_to_fp16(float x)
{
	return fp_downconvert<fp16_int_t,
	       FP16_BIAS, FP16_MANTISSA_BITS, FP16_EXPONENT_BITS, FP16_MAX_EXPONENT,
	       FP32_BIAS, FP32_MANTISSA_BITS, FP32_EXPONENT_BITS, FP32_MAX_EXPONENT>(x);
}

#endif

}  // namespace
