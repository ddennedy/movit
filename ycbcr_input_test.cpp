// Unit tests for YCbCrInput.
// FIXME: This class really ought to support mipmaps.

#include "test_util.h"
#include "gtest/gtest.h"
#include "ycbcr_input.h"

TEST(YCbCrInput, Simple444) {
	const int width = 1;
	const int height = 5;

	// Pure-color test inputs, calculated with the formulas in Rec. 601
	// section 2.5.4.
	unsigned char y[width * height] = {
		16, 235, 81, 145, 41,
	};
	unsigned char cb[width * height] = {
		128, 128, 90, 54, 240,
	};
	unsigned char cr[width * height] = {
		128, 128, 240, 34, 110,
	};
	float expected_data[4 * width * height] = {
		0.0, 0.0, 0.0, 1.0,
		1.0, 1.0, 1.0, 1.0,
		1.0, 0.0, 0.0, 1.0,
		0.0, 1.0, 0.0, 1.0,
		0.0, 0.0, 1.0, 1.0,
	};
	float out_data[4 * width * height];

	EffectChainTester tester(NULL, width, height);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_sRGB;

	YCbCrFormat ycbcr_format;
	ycbcr_format.luma_coefficients = YCBCR_REC_601;
	ycbcr_format.full_range = false;
	ycbcr_format.chroma_subsampling_x = 1;
	ycbcr_format.chroma_subsampling_y = 1;
	ycbcr_format.chroma_x_position = 0.5f;
	ycbcr_format.chroma_y_position = 0.5f;

	YCbCrInput *input = new YCbCrInput(format, ycbcr_format, width, height);
	input->set_pixel_data(0, y);
	input->set_pixel_data(1, cb);
	input->set_pixel_data(2, cr);
	tester.get_chain()->add_input(input);

	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_sRGB);

	// Y'CbCr isn't 100% accurate (the input values are rounded),
	// so we need some leeway.
	expect_equal(expected_data, out_data, 4 * width, height, 0.025, 0.002);
}

TEST(YCbCrInput, FullRangeRec601) {
	const int width = 1;
	const int height = 5;

	// Pure-color test inputs, calculated with the formulas in Rec. 601
	// section 2.5.4 but without the scaling factors applied
	// (so both R, G, B, Y, Cb and R vary from 0 to 255).
	unsigned char y[width * height] = {
		0, 255, 76, 150, 29,
	};
	unsigned char cb[width * height] = {
		128, 128, 85, 44, 255,
	};
	unsigned char cr[width * height] = {
		128, 128, 255, 21, 107,
	};
	float expected_data[4 * width * height] = {
		0.0, 0.0, 0.0, 1.0,
		1.0, 1.0, 1.0, 1.0,
		1.0, 0.0, 0.0, 1.0,
		0.0, 1.0, 0.0, 1.0,
		0.0, 0.0, 1.0, 1.0,
	};
	float out_data[4 * width * height];

	EffectChainTester tester(NULL, width, height);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_sRGB;

	YCbCrFormat ycbcr_format;
	ycbcr_format.luma_coefficients = YCBCR_REC_601;
	ycbcr_format.full_range = true;
	ycbcr_format.chroma_subsampling_x = 1;
	ycbcr_format.chroma_subsampling_y = 1;
	ycbcr_format.chroma_x_position = 0.5f;
	ycbcr_format.chroma_y_position = 0.5f;

	YCbCrInput *input = new YCbCrInput(format, ycbcr_format, width, height);
	input->set_pixel_data(0, y);
	input->set_pixel_data(1, cb);
	input->set_pixel_data(2, cr);
	tester.get_chain()->add_input(input);

	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_sRGB);

	// Y'CbCr isn't 100% accurate (the input values are rounded),
	// so we need some leeway.
	expect_equal(expected_data, out_data, 4 * width, height, 0.025, 0.002);
}
