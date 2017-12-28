// Unit tests for YCbCrConversionEffect. Mostly done by leveraging
// YCbCrInput and seeing that the right thing comes out at the
// other end.

#include <epoxy/gl.h>
#include <math.h>

#include "effect_chain.h"
#include "gtest/gtest.h"
#include "image_format.h"
#include "test_util.h"
#include "util.h"
#include "ycbcr_input.h"

namespace movit {

TEST(YCbCrConversionEffectTest, BasicInOut) {
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
	unsigned char expected_data[width * height * 4] = {
		// The same data, just rearranged.
		 16, 128, 128, 255,
		235, 128, 128, 255,
		 81,  90, 240, 255,
		145,  54,  34, 255,
		 41, 240, 110, 255
	};

	unsigned char out_data[width * height * 4];

	EffectChainTester tester(nullptr, width, height, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, GL_RGBA8);

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

	tester.add_ycbcr_output(format, OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED, ycbcr_format);

	YCbCrInput *input = new YCbCrInput(format, ycbcr_format, width, height);
	input->set_pixel_data(0, y);
	input->set_pixel_data(1, cb);
	input->set_pixel_data(2, cr);
	tester.get_chain()->add_input(input);

	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_sRGB);
	expect_equal(expected_data, out_data, 4 * width, height);
}

TEST(YCbCrConversionEffectTest, ClampToValidRange) {
	const int width = 1;
	const int height = 6;

	// Some out-of-range of at-range values.
	// Y should be clamped to 16-235 and Cb/Cr to 16-240.
	// (Alpha should still be 255.)
	unsigned char y[width * height] = {
		0, 10, 16, 235, 240, 255
	};
	unsigned char cb[width * height] = {
		0, 10, 16, 235, 240, 255,
	};
	unsigned char cr[width * height] = {
		255, 240, 235, 16, 10, 0,
	};
	unsigned char expected_data[width * height * 4] = {
		16, 16, 240, 255,
		16, 16, 240, 255,
		16, 16, 235, 255,
		235, 235, 16, 255,
		235, 240, 16, 255,
		235, 240, 16, 255,
	};

	unsigned char out_data[width * height * 4];

	EffectChainTester tester(nullptr, width, height, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, GL_RGBA8);

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

	tester.add_ycbcr_output(format, OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED, ycbcr_format);

	YCbCrInput *input = new YCbCrInput(format, ycbcr_format, width, height);
	input->set_pixel_data(0, y);
	input->set_pixel_data(1, cb);
	input->set_pixel_data(2, cr);
	tester.get_chain()->add_input(input);

	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_sRGB);
	expect_equal(expected_data, out_data, 4 * width, height);
}

TEST(YCbCrConversionEffectTest, LimitedRangeToFullRange) {
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
	unsigned char expected_data[width * height * 4] = {
		// Range now from 0-255 for all components, and values in-between
		// also adjusted a bit.
		  0, 128, 128, 255,
		255, 128, 128, 255,
		 76,  85, 255, 255,
		150,  44,  21, 255,
		 29, 255, 108, 255
	};

	unsigned char out_data[width * height * 4];

	EffectChainTester tester(nullptr, width, height, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, GL_RGBA8);

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

	tester.add_ycbcr_output(format, OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED, ycbcr_format);

	ycbcr_format.full_range = false;
	YCbCrInput *input = new YCbCrInput(format, ycbcr_format, width, height);
	input->set_pixel_data(0, y);
	input->set_pixel_data(1, cb);
	input->set_pixel_data(2, cr);
	tester.get_chain()->add_input(input);

	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_sRGB);

	// This specific data element has the correct value (110-128)*(255/224) + 128 = 107.509,
	// which rounds the wrong way on some cards. In normal use, we detect this and round off
	// in DitherEffect instead (so it's not a problem in pratice), but in unit tests like this,
	// we don't run with dither, so we simply fudge this one value instead.
	if (out_data[18] == 107) {
		out_data[18] = 108;
	}

	expect_equal(expected_data, out_data, 4 * width, height);
}

TEST(YCbCrConversionEffectTest, PlanarOutput) {
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

	unsigned char out_y[width * height], out_cb[width * height], out_cr[width * height];

	EffectChainTester tester(nullptr, width, height, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, GL_RGBA8);

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

	tester.add_ycbcr_output(format, OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED, ycbcr_format, YCBCR_OUTPUT_PLANAR);

	YCbCrInput *input = new YCbCrInput(format, ycbcr_format, width, height);
	input->set_pixel_data(0, y);
	input->set_pixel_data(1, cb);
	input->set_pixel_data(2, cr);
	tester.get_chain()->add_input(input);

	tester.run({out_y, out_cb, out_cr}, GL_RED, COLORSPACE_sRGB, GAMMA_sRGB);
	expect_equal(y, out_y, width, height);
	expect_equal(cb, out_cb, width, height);
	expect_equal(cr, out_cr, width, height);
}

TEST(YCbCrConversionEffectTest, SplitLumaAndChroma) {
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

	// The R and A data, rearranged. Note: The G and B channels
	// (the middle columns) are undefined. If we change the behavior,
	// the test will need to be updated, but a failure is expected.
	unsigned char expected_y[width * height * 4] = {
		 16, /*undefined:*/  16, /*undefined:*/  16, 255,
		235, /*undefined:*/ 235, /*undefined:*/ 235, 255,
		 81, /*undefined:*/  81, /*undefined:*/  81, 255,
		145, /*undefined:*/ 145, /*undefined:*/ 145, 255,
		 41, /*undefined:*/  41, /*undefined:*/  41, 255,
	};

	// Just the Cb and Cr data, rearranged. The B and A channels
	// are undefined, as below.
	unsigned char expected_cbcr[width * height * 4] = {
		128, 128, /*undefined:*/ 128, /*undefined:*/ 255,
		128, 128, /*undefined:*/ 128, /*undefined:*/ 255,
		 90, 240, /*undefined:*/ 240, /*undefined:*/ 255,
		 54,  34, /*undefined:*/  34, /*undefined:*/ 255,
		240, 110, /*undefined:*/ 110, /*undefined:*/ 255,
	};

	unsigned char out_y[width * height * 4], out_cbcr[width * height * 4];

	EffectChainTester tester(nullptr, width, height, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, GL_RGBA8);

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

	tester.add_ycbcr_output(format, OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED, ycbcr_format, YCBCR_OUTPUT_SPLIT_Y_AND_CBCR);

	YCbCrInput *input = new YCbCrInput(format, ycbcr_format, width, height);
	input->set_pixel_data(0, y);
	input->set_pixel_data(1, cb);
	input->set_pixel_data(2, cr);
	tester.get_chain()->add_input(input);

	tester.run(std::vector<unsigned char *>{out_y, out_cbcr}, GL_RGBA, COLORSPACE_sRGB, GAMMA_sRGB);
	expect_equal(expected_y, out_y, width * 4, height);
	expect_equal(expected_cbcr, out_cbcr, width * 4, height);
}

TEST(YCbCrConversionEffectTest, OutputChunkyAndRGBA) {
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
	unsigned char expected_ycbcr[width * height * 4] = {
		// The same data, just rearranged.
		 16, 128, 128, 255,
		235, 128, 128, 255,
		 81,  90, 240, 255,
		145,  54,  34, 255,
		 41, 240, 110, 255
	};
	unsigned char expected_rgba[width * height * 4] = {
		  0,   0,   0, 255,
		255, 255, 255, 255,
		255,   0,   0, 255,
		  0, 255,   0, 255,
		  0,   0, 255, 255,
	};

	unsigned char out_ycbcr[width * height * 4];
	unsigned char out_rgba[width * height * 4];

	EffectChainTester tester(nullptr, width, height, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, GL_RGBA8);

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

	tester.add_output(format, OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	tester.add_ycbcr_output(format, OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED, ycbcr_format);

	YCbCrInput *input = new YCbCrInput(format, ycbcr_format, width, height);
	input->set_pixel_data(0, y);
	input->set_pixel_data(1, cb);
	input->set_pixel_data(2, cr);
	tester.get_chain()->add_input(input);

	// Note: We don't test that the values actually get dithered,
	// just that the shader compiles and doesn't mess up badly.
	tester.get_chain()->set_dither_bits(8);

	tester.run(std::vector<unsigned char *>{out_ycbcr, out_rgba}, GL_RGBA, COLORSPACE_sRGB, GAMMA_sRGB);
	expect_equal(expected_ycbcr, out_ycbcr, width * 4, height);

	// Y'CbCr isn't 100% accurate (the input values are rounded),
	// so we need some leeway.
	expect_equal(expected_rgba, out_rgba, 4 * width, height, 7, 255 * 0.002);
}

TEST(YCbCrConversionEffectTest, MultipleOutputsAndRGBA) {
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
	unsigned char expected_ycbcr[width * height * 4] = {
		// The same data, just rearranged.
		 16, 128, 128, 255,
		235, 128, 128, 255,
		 81,  90, 240, 255,
		145,  54,  34, 255,
		 41, 240, 110, 255
	};
	unsigned char expected_rgba[width * height * 4] = {
		  0,   0,   0, 255,
		255, 255, 255, 255,
		255,   0,   0, 255,
		  0, 255,   0, 255,
		  0,   0, 255, 255,
	};

	unsigned char out_ycbcr[width * height * 4];
	unsigned char out_y[width * height * 4];
	unsigned char out_cbcr[width * height * 4];
	unsigned char out_rgba[width * height * 4];

	EffectChainTester tester(nullptr, width, height, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, GL_RGBA8);

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

	tester.add_output(format, OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	tester.add_ycbcr_output(format, OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED, ycbcr_format);
	tester.add_ycbcr_output(format, OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED, ycbcr_format, YCBCR_OUTPUT_SPLIT_Y_AND_CBCR);

	YCbCrInput *input = new YCbCrInput(format, ycbcr_format, width, height);
	input->set_pixel_data(0, y);
	input->set_pixel_data(1, cb);
	input->set_pixel_data(2, cr);
	tester.get_chain()->add_input(input);

	// Note: We don't test that the values actually get dithered,
	// just that the shader compiles and doesn't mess up badly.
	tester.get_chain()->set_dither_bits(8);

	tester.run({out_ycbcr, out_y, out_cbcr, out_rgba}, GL_RGBA, COLORSPACE_sRGB, GAMMA_sRGB);
	expect_equal(expected_ycbcr, out_ycbcr, width * 4, height);

	// Check that the extra Y' and CbCr outputs also are fine.
	for (unsigned i = 0; i < width * height; ++i) {
		out_ycbcr[i * 4] = out_y[i * 4];
		out_ycbcr[i * 4 + 1] = out_cbcr[i * 4 + 0];
		out_ycbcr[i * 4 + 2] = out_cbcr[i * 4 + 1];
	}
	expect_equal(expected_ycbcr, out_ycbcr, width * 4, height);

	// Y'CbCr isn't 100% accurate (the input values are rounded),
	// so we need some leeway.
	expect_equal(expected_rgba, out_rgba, 4 * width, height, 7, 255 * 0.002);
}

// Very similar to PlanarOutput.
TEST(YCbCrConversionEffectTest, ChangeOutputFormat) {
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

	unsigned char out_y[width * height], out_cb[width * height], out_cr[width * height];

	EffectChainTester tester(nullptr, width, height, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, GL_RGBA8);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_sRGB;

	YCbCrFormat ycbcr_format;
	ycbcr_format.luma_coefficients = YCBCR_REC_709;  // Deliberately wrong at first.
	ycbcr_format.full_range = false;
	ycbcr_format.num_levels = 256;
	ycbcr_format.chroma_subsampling_x = 1;
	ycbcr_format.chroma_subsampling_y = 1;
	ycbcr_format.cb_x_position = 0.5f;
	ycbcr_format.cb_y_position = 0.5f;
	ycbcr_format.cr_x_position = 0.5f;
	ycbcr_format.cr_y_position = 0.5f;

	tester.add_ycbcr_output(format, OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED, ycbcr_format, YCBCR_OUTPUT_PLANAR);

	ycbcr_format.luma_coefficients = YCBCR_REC_601;
	YCbCrInput *input = new YCbCrInput(format, ycbcr_format, width, height);
	input->set_pixel_data(0, y);
	input->set_pixel_data(1, cb);
	input->set_pixel_data(2, cr);
	tester.get_chain()->add_input(input);

	tester.run({out_y, out_cb, out_cr}, GL_RED, COLORSPACE_sRGB, GAMMA_sRGB);

	// Now change the output format to match what we gave the input, and re-run.
	tester.get_chain()->change_ycbcr_output_format(ycbcr_format);
	tester.run({out_y, out_cb, out_cr}, GL_RED, COLORSPACE_sRGB, GAMMA_sRGB);

	expect_equal(y, out_y, width, height);
	expect_equal(cb, out_cb, width, height);
	expect_equal(cr, out_cr, width, height);
}

TEST(YCbCrConversionEffectTest, TenBitOutput) {
	const int width = 1;
	const int height = 5;

	// Pure-color test inputs.
	float data[width * height * 4] = {
		0.0f, 0.0f, 0.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
	};
	uint32_t out_data[width * height];
	int expanded_out_data[width * height * 4];
	int expected_data[width * height * 4] = {
		// Expected results, calculated using formulas 3.2, 3.3 and 3.4
		// from Rec. 709. (Except the first two, which are obvious
		// given the 64–940 range of luminance.)
		 64, 512, 512, 3,
		940, 512, 512, 3,
		250, 409, 960, 3,
		691, 167, 105, 3,
		127, 960, 471, 3,
	};

	EffectChainTester tester(nullptr, width, height, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, GL_RGB10_A2);
	tester.add_input(data, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_sRGB);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_sRGB;

	YCbCrFormat ycbcr_format;
	ycbcr_format.luma_coefficients = YCBCR_REC_709;
	ycbcr_format.full_range = false;
	ycbcr_format.num_levels = 1024;
	ycbcr_format.chroma_subsampling_x = 1;
	ycbcr_format.chroma_subsampling_y = 1;
	ycbcr_format.cb_x_position = 0.5f;
	ycbcr_format.cb_y_position = 0.5f;
	ycbcr_format.cr_x_position = 0.5f;
	ycbcr_format.cr_y_position = 0.5f;

	tester.add_ycbcr_output(format, OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED, ycbcr_format);
	tester.run_10_10_10_2(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_sRGB);

	// Unpack 10:10:10:2 to 32:32:32:32.
	for (unsigned i = 0; i < width * height; ++i) {
		expanded_out_data[i * 4 + 0] = out_data[i] & 0x3ff;
		expanded_out_data[i * 4 + 1] = (out_data[i] >> 10) & 0x3ff;
		expanded_out_data[i * 4 + 2] = (out_data[i] >> 20) & 0x3ff;
		expanded_out_data[i * 4 + 3] = (out_data[i] >> 30);
	}
	expect_equal(expected_data, expanded_out_data, 4 * width, height);
}

TEST(YCbCrConversionEffectTest, TenBitOutputInSixteen) {
	const int width = 1;
	const int height = 5;

	// Same test inputs and outputs as TenBitOutput, except that alpha
	// is 16 bits instead of two.
	float data[width * height * 4] = {
		0.0f, 0.0f, 0.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f,
	};
	uint16_t out_data[width * height * 4];
	uint16_t expected_data[width * height * 4] = {
		 64, 512, 512, 65535,
		940, 512, 512, 65535,
		250, 409, 960, 65535,
		691, 167, 105, 65535,
		127, 960, 471, 65535,
	};

	EffectChainTester tester(nullptr, width, height, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, GL_RGBA16);
	tester.add_input(data, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_sRGB);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_sRGB;

	YCbCrFormat ycbcr_format;
	ycbcr_format.luma_coefficients = YCBCR_REC_709;
	ycbcr_format.full_range = false;
	ycbcr_format.num_levels = 1024;
	ycbcr_format.chroma_subsampling_x = 1;
	ycbcr_format.chroma_subsampling_y = 1;
	ycbcr_format.cb_x_position = 0.5f;
	ycbcr_format.cb_y_position = 0.5f;
	ycbcr_format.cr_x_position = 0.5f;
	ycbcr_format.cr_y_position = 0.5f;

	tester.add_ycbcr_output(format, OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED, ycbcr_format, YCBCR_OUTPUT_INTERLEAVED, GL_UNSIGNED_SHORT);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_sRGB);

	// Add some slight leeway for the benefit of cards that don't
	// round correctly (would be fixed by DitherEffect if we had dither).
	expect_equal(expected_data, out_data, 4 * width, height, 2);
}

}  // namespace movit
