#include "fp16.h"

#include <math.h>
#include <gtest/gtest.h>

namespace movit {

TEST(FP16Test, Simple) {
	EXPECT_EQ(0x0000, fp64_to_fp16(0.0));
	EXPECT_DOUBLE_EQ(0.0, fp16_to_fp64(0x0000));

	EXPECT_EQ(0x3c00, fp64_to_fp16(1.0));
	EXPECT_DOUBLE_EQ(1.0, fp16_to_fp64(0x3c00));

	EXPECT_EQ(0x3555, fp64_to_fp16(1.0 / 3.0));
	EXPECT_DOUBLE_EQ(0.333251953125, fp16_to_fp64(0x3555));
}

TEST(FP16Test, RoundToNearestEven) {
	ASSERT_DOUBLE_EQ(1.0, fp16_to_fp64(0x3c00));

	double x0 = fp16_to_fp64(0x3c00);
	double x1 = fp16_to_fp64(0x3c01);
	double x2 = fp16_to_fp64(0x3c02);
	double x3 = fp16_to_fp64(0x3c03);
	double x4 = fp16_to_fp64(0x3c04);

	EXPECT_EQ(0x3c00, fp64_to_fp16(0.5 * (x0 + x1)));
	EXPECT_EQ(0x3c02, fp64_to_fp16(0.5 * (x1 + x2)));
	EXPECT_EQ(0x3c02, fp64_to_fp16(0.5 * (x2 + x3)));
	EXPECT_EQ(0x3c04, fp64_to_fp16(0.5 * (x3 + x4)));
}

union fp64 {
	double f;
	unsigned long long ll;
};
union fp32 {
	float f;
	unsigned int u;
};

TEST(FP16Test, NaN) {
	// Ignore the sign bit.
	EXPECT_EQ(0x7e00, fp64_to_fp16(0.0 / 0.0) & 0x7fff);
	EXPECT_TRUE(isnan(fp16_to_fp64(0xfe00)));

	fp64 borderline_inf;
	borderline_inf.ll = 0x7ff0000000000000ull;
	fp64 borderline_nan;
	borderline_nan.ll = 0x7ff0000000000001ull;

	ASSERT_FALSE(isfinite(borderline_inf.f));
	ASSERT_FALSE(isnan(borderline_inf.f));

	ASSERT_FALSE(isfinite(borderline_nan.f));
	ASSERT_TRUE(isnan(borderline_nan.f));

	double borderline_inf_roundtrip = fp16_to_fp64(fp64_to_fp16(borderline_inf.f));
	double borderline_nan_roundtrip = fp16_to_fp64(fp64_to_fp16(borderline_nan.f));

	EXPECT_FALSE(isfinite(borderline_inf_roundtrip));
	EXPECT_FALSE(isnan(borderline_inf_roundtrip));

	EXPECT_FALSE(isfinite(borderline_nan_roundtrip));
	EXPECT_TRUE(isnan(borderline_nan_roundtrip));
}

TEST(FP16Test, Denormals) {
	const double smallest_fp16_denormal = 5.9604644775390625e-08;
	EXPECT_EQ(0x0001, fp64_to_fp16(smallest_fp16_denormal));
	EXPECT_EQ(0x0000, fp64_to_fp16(0.5 * smallest_fp16_denormal));  // Round-to-even.
	EXPECT_EQ(0x0001, fp64_to_fp16(0.51 * smallest_fp16_denormal));
	EXPECT_EQ(0x0002, fp64_to_fp16(1.5 * smallest_fp16_denormal));

	const double smallest_fp16_non_denormal = 6.103515625e-05;
	EXPECT_EQ(0x0400, fp64_to_fp16(smallest_fp16_non_denormal));
	EXPECT_EQ(0x0400, fp64_to_fp16(smallest_fp16_non_denormal - 0.5 * smallest_fp16_denormal));  // Round-to-even.
	EXPECT_EQ(0x03ff, fp64_to_fp16(smallest_fp16_non_denormal - smallest_fp16_denormal));
}

// Randomly test a large number of fp64 -> fp32 conversions, comparing
// against the FPU.
TEST(FP16Test, FP32ReferenceDownconvert) {
	srand(12345);

	for (int i = 0; i < 1000000; ++i) {
		unsigned r1 = rand();
		unsigned r2 = rand();
		unsigned r3 = rand();
		union fp64 src;
		union fp32 reference, result;

		src.ll = (((unsigned long long)r1) << 33) ^ ((unsigned long long)r2 << 16) ^ r3;
		reference.f = float(src.f);
		result.u = fp64_to_fp32(src.f);

		EXPECT_EQ(isnan(result.f), isnan(reference.f));
		if (!isnan(result.f)) {
			EXPECT_EQ(result.u, reference.u)
			    << src.f << " got rounded to " << result.u << " (" << result.f << ")";
		}
	}
}

// Randomly test a large number of fp32 -> fp64 conversions, comparing
// against the FPU.
TEST(FP16Test, FP32ReferenceUpconvert) {
	srand(12345);

	for (int i = 0; i < 1000000; ++i) {
		unsigned r1 = rand();
		unsigned r2 = rand();
		union fp32 src;
		union fp64 reference, result;

		src.u = ((unsigned long long)r1 << 16) ^ r2;
		reference.f = double(src.f);
		result.f = fp32_to_fp64(src.u);

		EXPECT_EQ(isnan(result.f), isnan(reference.f));
		if (!isnan(result.f)) {
			EXPECT_EQ(result.ll, reference.ll)
			    << src.f << " got converted to " << result.ll << " (" << result.f << ")";
		}
	}
}

}  // namespace movit
