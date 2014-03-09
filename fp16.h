#ifndef _MOVIT_FP16_H
#define _MOVIT_FP16_H 1

// Code for converting to and from fp16 (from fp64), without any particular
// machine support, with proper IEEE round-to-even behavior (and correct
// handling of NaNs and infinities). This is needed because some OpenGL
// drivers don't properly round off when asked to convert data themselves.
//
// These routines are not particularly fast.

namespace movit {

typedef unsigned int fp32_int_t;
typedef unsigned short fp16_int_t;

double fp16_to_fp64(fp16_int_t x);
fp16_int_t fp64_to_fp16(double x);

// These are not very useful by themselves, but are implemented using the same
// code as the fp16 ones (just with different constants), so they are useful
// for verifying against the FPU in unit tests.
double fp32_to_fp64(fp32_int_t x);
fp32_int_t fp64_to_fp32(double x);

}  // namespace movit

#endif  // _MOVIT_FP16_H
