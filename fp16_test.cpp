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

TEST(FP16Test, NaN) {
	EXPECT_EQ(0xfe00, fp64_to_fp16(0.0 / 0.0));
	EXPECT_TRUE(isnan(fp16_to_fp64(0xfe00)));
}

union fp64 {
	double f;
	unsigned long long ll;
};
union fp32 {
	float f;
	unsigned int u;
};

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
