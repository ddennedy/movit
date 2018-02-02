#include "fp16.h"

#include <cmath>
#include <gtest/gtest.h>

namespace movit {
namespace {

fp16_int_t make_fp16(unsigned short x)
{
	fp16_int_t ret;
	ret.val = x;
	return ret;
}

}  // namespace

TEST(FP16Test, Simple) {
	EXPECT_EQ(0x0000, fp32_to_fp16(0.0).val);
	EXPECT_DOUBLE_EQ(0.0, fp16_to_fp32(make_fp16(0x0000)));

	EXPECT_EQ(0x3c00, fp32_to_fp16(1.0).val);
	EXPECT_DOUBLE_EQ(1.0, fp16_to_fp32(make_fp16(0x3c00)));

	EXPECT_EQ(0x3555, fp32_to_fp16(1.0 / 3.0).val);
	EXPECT_DOUBLE_EQ(0.333251953125, fp16_to_fp32(make_fp16(0x3555)));
}

TEST(FP16Test, RoundToNearestEven) {
	ASSERT_DOUBLE_EQ(1.0, fp16_to_fp32(make_fp16(0x3c00)));

	double x0 = fp16_to_fp32(make_fp16(0x3c00));
	double x1 = fp16_to_fp32(make_fp16(0x3c01));
	double x2 = fp16_to_fp32(make_fp16(0x3c02));
	double x3 = fp16_to_fp32(make_fp16(0x3c03));
	double x4 = fp16_to_fp32(make_fp16(0x3c04));

	EXPECT_EQ(0x3c00, fp32_to_fp16(0.5 * (x0 + x1)).val);
	EXPECT_EQ(0x3c02, fp32_to_fp16(0.5 * (x1 + x2)).val);
	EXPECT_EQ(0x3c02, fp32_to_fp16(0.5 * (x2 + x3)).val);
	EXPECT_EQ(0x3c04, fp32_to_fp16(0.5 * (x3 + x4)).val);
}

union fp64 {
	double f;
	unsigned long long ll;
};

#ifdef __F16C__
union fp32 {
	float f;
	unsigned int u;
};
#endif

TEST(FP16Test, NaN) {
	// Ignore the sign bit.
	EXPECT_EQ(0x7e00, fp32_to_fp16(0.0 / 0.0).val & 0x7fff);
	EXPECT_TRUE(std::isnan(fp16_to_fp32(make_fp16(0xfe00))));

	fp32 borderline_inf;
	borderline_inf.u = 0x7f800000ull;
	fp32 borderline_nan;
	borderline_nan.u = 0x7f800001ull;

	ASSERT_FALSE(std::isfinite(borderline_inf.f));
	ASSERT_FALSE(std::isnan(borderline_inf.f));

	ASSERT_FALSE(std::isfinite(borderline_nan.f));
	ASSERT_TRUE(std::isnan(borderline_nan.f));

	double borderline_inf_roundtrip = fp16_to_fp32(fp32_to_fp16(borderline_inf.f));
	double borderline_nan_roundtrip = fp16_to_fp32(fp32_to_fp16(borderline_nan.f));

	EXPECT_FALSE(std::isfinite(borderline_inf_roundtrip));
	EXPECT_FALSE(std::isnan(borderline_inf_roundtrip));

	EXPECT_FALSE(std::isfinite(borderline_nan_roundtrip));
	EXPECT_TRUE(std::isnan(borderline_nan_roundtrip));
}

TEST(FP16Test, Denormals) {
	const double smallest_fp16_denormal = 5.9604644775390625e-08;
	EXPECT_EQ(0x0001, fp32_to_fp16(smallest_fp16_denormal).val);
	EXPECT_EQ(0x0000, fp32_to_fp16(0.5 * smallest_fp16_denormal).val);  // Round-to-even.
	EXPECT_EQ(0x0001, fp32_to_fp16(0.51 * smallest_fp16_denormal).val);
	EXPECT_EQ(0x0002, fp32_to_fp16(1.5 * smallest_fp16_denormal).val);

	const double smallest_fp16_non_denormal = 6.103515625e-05;
	EXPECT_EQ(0x0400, fp32_to_fp16(smallest_fp16_non_denormal).val);
	EXPECT_EQ(0x0400, fp32_to_fp16(smallest_fp16_non_denormal - 0.5 * smallest_fp16_denormal).val);  // Round-to-even.
	EXPECT_EQ(0x03ff, fp32_to_fp16(smallest_fp16_non_denormal - smallest_fp16_denormal).val);
}

}  // namespace movit
