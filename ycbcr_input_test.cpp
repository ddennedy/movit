// Unit tests for YCbCrInput.

#include <epoxy/gl.h>
#include <stddef.h>

#include "effect_chain.h"
#include "gtest/gtest.h"
#include "test_util.h"
#include "util.h"
#include "ycbcr_input.h"

namespace movit {

TEST(YCbCrInputTest, Simple444) {
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
	ycbcr_format.num_levels = 256;
	ycbcr_format.chroma_subsampling_x = 1;
	ycbcr_format.chroma_subsampling_y = 1;
	ycbcr_format.cb_x_position = 0.5f;
	ycbcr_format.cb_y_position = 0.5f;
	ycbcr_format.cr_x_position = 0.5f;
	ycbcr_format.cr_y_position = 0.5f;

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

TEST(YCbCrInputTest, FullRangeRec601) {
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
	ycbcr_format.num_levels = 256;
	ycbcr_format.chroma_subsampling_x = 1;
	ycbcr_format.chroma_subsampling_y = 1;
	ycbcr_format.cb_x_position = 0.5f;
	ycbcr_format.cb_y_position = 0.5f;
	ycbcr_format.cr_x_position = 0.5f;
	ycbcr_format.cr_y_position = 0.5f;

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

TEST(YCbCrInputTest, Rec709) {
	const int width = 1;
	const int height = 5;

	// Pure-color test inputs, calculated with the formulas in Rec. 709
	// page 19, items 3.4 and 3.5.
	unsigned char y[width * height] = {
		16, 235, 63, 173, 32, 
	};
	unsigned char cb[width * height] = {
		128, 128, 102, 42, 240,
	};
	unsigned char cr[width * height] = {
		128, 128, 240, 26, 118,
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
	ycbcr_format.luma_coefficients = YCBCR_REC_709;
	ycbcr_format.full_range = false;
	ycbcr_format.num_levels = 256;
	ycbcr_format.chroma_subsampling_x = 1;
	ycbcr_format.chroma_subsampling_y = 1;
	ycbcr_format.cb_x_position = 0.5f;
	ycbcr_format.cb_y_position = 0.5f;
	ycbcr_format.cr_x_position = 0.5f;
	ycbcr_format.cr_y_position = 0.5f;

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

TEST(YCbCrInputTest, Rec2020) {
	const int width = 1;
	const int height = 5;

	// Pure-color test inputs, calculated with the formulas in Rec. 2020
	// page 4, tables 4 and 5 (for conventional non-constant luminance).
	// Note that we still use 8-bit inputs, even though Rec. 2020 is only
	// defined for 10- and 12-bit.
	unsigned char y[width * height] = {
		16, 235, 74, 164, 29,
	};
	unsigned char cb[width * height] = {
		128, 128, 97, 47, 240,
	};
	unsigned char cr[width * height] = {
		128, 128, 240, 25, 119,
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
	ycbcr_format.luma_coefficients = YCBCR_REC_2020;
	ycbcr_format.full_range = false;
	ycbcr_format.num_levels = 256;
	ycbcr_format.chroma_subsampling_x = 1;
	ycbcr_format.chroma_subsampling_y = 1;
	ycbcr_format.cb_x_position = 0.5f;
	ycbcr_format.cb_y_position = 0.5f;
	ycbcr_format.cr_x_position = 0.5f;
	ycbcr_format.cr_y_position = 0.5f;

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

TEST(YCbCrInputTest, Subsampling420) {
	const int width = 4;
	const int height = 4;

	unsigned char y[width * height] = {
		126, 126, 126, 126,
		126, 126, 126, 126,
		126, 126, 126, 126,
		126, 126, 126, 126,
	};
	unsigned char cb[(width/2) * (height/2)] = {
		64, 128,
		128, 192,
	};
	unsigned char cr[(width/2) * (height/2)] = {
		128, 128,
		128, 128,
	};

	// Note: This is only the blue channel. The chroma samples (with associated
	// values for blue) are marked off in comments.
	float expected_data[width * height] = {
		0.000, 0.125, 0.375, 0.500, 
		 /* 0.0 */      /* 0.5 */
		0.125, 0.250, 0.500, 0.625,

		0.375, 0.500, 0.750, 0.875,
		 /* 0.5 */      /* 1.0 */
		0.500, 0.625, 0.875, 1.000,
	};
	float out_data[width * height];

	EffectChainTester tester(NULL, width, height);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_sRGB;

	YCbCrFormat ycbcr_format;
	ycbcr_format.luma_coefficients = YCBCR_REC_601;
	ycbcr_format.full_range = false;
	ycbcr_format.num_levels = 256;
	ycbcr_format.chroma_subsampling_x = 2;
	ycbcr_format.chroma_subsampling_y = 2;
	ycbcr_format.cb_x_position = 0.5f;
	ycbcr_format.cb_y_position = 0.5f;
	ycbcr_format.cr_x_position = 0.5f;
	ycbcr_format.cr_y_position = 0.5f;

	YCbCrInput *input = new YCbCrInput(format, ycbcr_format, width, height);
	input->set_pixel_data(0, y);
	input->set_pixel_data(1, cb);
	input->set_pixel_data(2, cr);
	tester.get_chain()->add_input(input);

	tester.run(out_data, GL_BLUE, COLORSPACE_sRGB, GAMMA_sRGB);

	// Y'CbCr isn't 100% accurate (the input values are rounded),
	// so we need some leeway.
	expect_equal(expected_data, out_data, width, height, 0.01, 0.001);
}

TEST(YCbCrInputTest, Subsampling420WithNonCenteredSamples) {
	const int width = 4;
	const int height = 4;

	unsigned char y[width * height] = {
		126, 126, 126, 126,
		126, 126, 126, 126,
		126, 126, 126, 126,
		126, 126, 126, 126,
	};
	unsigned char cb[(width/2) * (height/2)] = {
		64, 128,
		128, 192,
	};
	unsigned char cr[(width/2) * (height/2)] = {
		128, 128,
		128, 128,
	};

	// Note: This is only the blue channel. The chroma samples (with associated
	// values for blue) are marked off in comments.
	float expected_data[width * height] = {
		   0.000, 0.250, 0.500, 0.500, 
		/* 0.0 */     /* 0.5 */
		   0.125, 0.375, 0.625, 0.625,

		   0.375, 0.625, 0.875, 0.875,
		/* 0.5 */     /* 1.0 */
		   0.500, 0.750, 1.000, 1.000,
	};
	float out_data[width * height];

	EffectChainTester tester(NULL, width, height);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_sRGB;

	YCbCrFormat ycbcr_format;
	ycbcr_format.luma_coefficients = YCBCR_REC_601;
	ycbcr_format.full_range = false;
	ycbcr_format.num_levels = 256;
	ycbcr_format.chroma_subsampling_x = 2;
	ycbcr_format.chroma_subsampling_y = 2;
	ycbcr_format.cb_x_position = 0.0f;
	ycbcr_format.cb_y_position = 0.5f;
	ycbcr_format.cr_x_position = 0.0f;
	ycbcr_format.cr_y_position = 0.5f;

	YCbCrInput *input = new YCbCrInput(format, ycbcr_format, width, height);
	input->set_pixel_data(0, y);
	input->set_pixel_data(1, cb);
	input->set_pixel_data(2, cr);
	tester.get_chain()->add_input(input);

	tester.run(out_data, GL_BLUE, COLORSPACE_sRGB, GAMMA_sRGB);

	// Y'CbCr isn't 100% accurate (the input values are rounded),
	// so we need some leeway.
	expect_equal(expected_data, out_data, width, height, 0.01, 0.001);
}

// Yes, some 4:2:2 formats actually have this craziness.
TEST(YCbCrInputTest, DifferentCbAndCrPositioning) {
	const int width = 4;
	const int height = 4;

	unsigned char y[width * height] = {
		126, 126, 126, 126,
		126, 126, 126, 126,
		126, 126, 126, 126,
		126, 126, 126, 126,
	};
	unsigned char cb[(width/2) * height] = {
		64, 128,
		128, 192,
		128, 128,
		128, 128,
	};
	unsigned char cr[(width/2) * height] = {
		48, 128,
		128, 208,
		128, 128,
		128, 128,
	};

	// Chroma samples in this csae are always co-sited with a luma sample;
	// their associated color values and position are marked off in comments.
	float expected_data_blue[width * height] = {
		   0.000 /* 0.0 */, 0.250,           0.500 /* 0.5 */, 0.500, 
		   0.500 /* 0.5 */, 0.750,           1.000 /* 1.0 */, 1.000, 
		   0.500 /* 0.5 */, 0.500,           0.500 /* 0.5 */, 0.500, 
		   0.500 /* 0.5 */, 0.500,           0.500 /* 0.5 */, 0.500, 
	};
	float expected_data_red[width * height] = {
		   0.000,           0.000 /* 0.0 */, 0.250,           0.500 /* 0.5 */, 
		   0.500,           0.500 /* 0.5 */, 0.750,           1.000 /* 1.0 */, 
		   0.500,           0.500 /* 0.5 */, 0.500,           0.500 /* 0.5 */, 
		   0.500,           0.500 /* 0.5 */, 0.500,           0.500 /* 0.5 */, 
	};
	float out_data[width * height];

	EffectChainTester tester(NULL, width, height);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_sRGB;

	YCbCrFormat ycbcr_format;
	ycbcr_format.luma_coefficients = YCBCR_REC_601;
	ycbcr_format.full_range = false;
	ycbcr_format.num_levels = 256;
	ycbcr_format.chroma_subsampling_x = 2;
	ycbcr_format.chroma_subsampling_y = 1;
	ycbcr_format.cb_x_position = 0.0f;
	ycbcr_format.cb_y_position = 0.5f;
	ycbcr_format.cr_x_position = 1.0f;
	ycbcr_format.cr_y_position = 0.5f;

	YCbCrInput *input = new YCbCrInput(format, ycbcr_format, width, height);
	input->set_pixel_data(0, y);
	input->set_pixel_data(1, cb);
	input->set_pixel_data(2, cr);
	tester.get_chain()->add_input(input);

	// Y'CbCr isn't 100% accurate (the input values are rounded),
	// so we need some leeway.
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_sRGB);
	expect_equal(expected_data_red, out_data, width, height, 0.02, 0.002);

	tester.run(out_data, GL_BLUE, COLORSPACE_sRGB, GAMMA_sRGB);
	expect_equal(expected_data_blue, out_data, width, height, 0.01, 0.001);
}

TEST(YCbCrInputTest, PBO) {
	const int width = 1;
	const int height = 5;

	// Pure-color test inputs, calculated with the formulas in Rec. 601
	// section 2.5.4.
	unsigned char data[width * height * 3] = {
		16, 235, 81, 145, 41,
		128, 128, 90, 54, 240,
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

	GLuint pbo;
	glGenBuffers(1, &pbo);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, pbo);
	glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, width * height * 3, data, GL_STREAM_DRAW);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);

	EffectChainTester tester(NULL, width, height);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_sRGB;

	YCbCrFormat ycbcr_format;
	ycbcr_format.luma_coefficients = YCBCR_REC_601;
	ycbcr_format.full_range = false;
	ycbcr_format.num_levels = 256;
	ycbcr_format.chroma_subsampling_x = 1;
	ycbcr_format.chroma_subsampling_y = 1;
	ycbcr_format.cb_x_position = 0.5f;
	ycbcr_format.cb_y_position = 0.5f;
	ycbcr_format.cr_x_position = 0.5f;
	ycbcr_format.cr_y_position = 0.5f;

	YCbCrInput *input = new YCbCrInput(format, ycbcr_format, width, height);
	input->set_pixel_data(0, (unsigned char *)BUFFER_OFFSET(0), pbo);
	input->set_pixel_data(1, (unsigned char *)BUFFER_OFFSET(width * height), pbo);
	input->set_pixel_data(2, (unsigned char *)BUFFER_OFFSET(width * height * 2), pbo);
	tester.get_chain()->add_input(input);

	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_sRGB);

	// Y'CbCr isn't 100% accurate (the input values are rounded),
	// so we need some leeway.
	expect_equal(expected_data, out_data, 4 * width, height, 0.025, 0.002);

	glDeleteBuffers(1, &pbo);
}

}  // namespace movit
