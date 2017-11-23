// Unit tests for YCbCr422InterleavedInput.

#include <epoxy/gl.h>
#include <stddef.h>

#include <string>

#include "effect_chain.h"
#include "gtest/gtest.h"
#include "test_util.h"
#include "util.h"
#include "resize_effect.h"
#include "ycbcr_422interleaved_input.h"

using namespace std;

namespace movit {

// Adapted from the Simple444 test from YCbCrInputTest.
TEST(YCbCr422InterleavedInputTest, Simple422) {
	const int width = 2;
	const int height = 5;

	// Pure-color test inputs, calculated with the formulas in Rec. 601
        // section 2.5.4.
	unsigned char uyvy[width * height * 2] = {
		/*U=*/128, /*Y=*/ 16, /*V=*/128, /*Y=*/ 16,
		/*U=*/128, /*Y=*/235, /*V=*/128, /*Y=*/235,
		/*U=*/ 90, /*Y=*/ 81, /*V=*/240, /*Y=*/ 81,
		/*U=*/ 54, /*Y=*/145, /*V=*/ 34, /*Y=*/145,
		/*U=*/240, /*Y=*/ 41, /*V=*/110, /*Y=*/ 41,
	};

	float expected_data[4 * width * height] = {
		0.0, 0.0, 0.0, 1.0,   0.0, 0.0, 0.0, 1.0,
		1.0, 1.0, 1.0, 1.0,   1.0, 1.0, 1.0, 1.0,
		1.0, 0.0, 0.0, 1.0,   1.0, 0.0, 0.0, 1.0,
		0.0, 1.0, 0.0, 1.0,   0.0, 1.0, 0.0, 1.0,
		0.0, 0.0, 1.0, 1.0,   0.0, 0.0, 1.0, 1.0,
	};
	float out_data[4 * width * height];

	EffectChainTester tester(nullptr, width, height);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_sRGB;

	YCbCrFormat ycbcr_format;
	ycbcr_format.luma_coefficients = YCBCR_REC_601;
	ycbcr_format.full_range = false;
	ycbcr_format.num_levels = 256;
	ycbcr_format.chroma_subsampling_x = 2;
	ycbcr_format.chroma_subsampling_y = 1;
	ycbcr_format.cb_x_position = 0.0f;  // Doesn't really matter here, since Y is constant.
	ycbcr_format.cb_y_position = 0.5f;
	ycbcr_format.cr_x_position = 0.0f;
	ycbcr_format.cr_y_position = 0.5f;

	YCbCr422InterleavedInput *input = new YCbCr422InterleavedInput(format, ycbcr_format, width, height);
	input->set_pixel_data(uyvy);
	tester.get_chain()->add_input(input);

	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_sRGB);

	// Y'CbCr isn't 100% accurate (the input values are rounded),
	// so we need some leeway.
	expect_equal(expected_data, out_data, 4 * width, height, 0.025, 0.002);
}

// An effect that does nothing except changing its output sizes.
class VirtualResizeEffect : public Effect {
public:
	VirtualResizeEffect(int width, int height, int virtual_width, int virtual_height)
		: width(width),
		  height(height),
		  virtual_width(virtual_width),
		  virtual_height(virtual_height) {}
	string effect_type_id() const override { return "VirtualResizeEffect"; }
	string output_fragment_shader() override { return read_file("identity.frag"); }

	bool changes_output_size() const override { return true; }

	void get_output_size(unsigned *width, unsigned *height,
	                     unsigned *virtual_width, unsigned *virtual_height) const override {
		*width = this->width;
		*height = this->height;
		*virtual_width = this->virtual_width;
		*virtual_height = this->virtual_height;
	}

private:
	int width, height, virtual_width, virtual_height;
};

TEST(YCbCr422InterleavedInputTest, LumaLinearInterpolation) {
	const int width = 4;
	const int height = 1;
	const int out_width = width * 3;

	// Black, white, black and then gray.
	unsigned char uyvy[width * height * 2] = {
		/*U=*/128, /*Y=*/ 16,
		/*V=*/128, /*Y=*/235,
		/*U=*/128, /*Y=*/ 16,
		/*V=*/128, /*Y=*/128,
	};

	float expected_data[out_width * height] = {
		0.0, /**/0.0, 0.333, 0.667, /**/1.0, 0.667, 0.333, /**/0.0, 0.167, 0.333, /**/0.5, 0.5
	};
	float out_data[out_width * height];

	EffectChainTester tester(nullptr, out_width, height);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_sRGB;

	YCbCrFormat ycbcr_format;
	ycbcr_format.luma_coefficients = YCBCR_REC_601;
	ycbcr_format.full_range = false;
	ycbcr_format.num_levels = 256;
	ycbcr_format.chroma_subsampling_x = 2;
	ycbcr_format.chroma_subsampling_y = 1;
	ycbcr_format.cb_x_position = 0.0f;  // Doesn't really matter here, since U/V are constant.
	ycbcr_format.cb_y_position = 0.5f;
	ycbcr_format.cr_x_position = 0.0f;
	ycbcr_format.cr_y_position = 0.5f;

	YCbCr422InterleavedInput *input = new YCbCr422InterleavedInput(format, ycbcr_format, width, height);
	input->set_pixel_data(uyvy);
	tester.get_chain()->add_input(input);
	tester.get_chain()->add_effect(new VirtualResizeEffect(out_width, height, out_width, height));

	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_sRGB);

	// Y'CbCr isn't 100% accurate (the input values are rounded),
	// so we need some leeway.
	expect_equal(expected_data, out_data, out_width, height, 0.025, 0.002);
}

// Adapted from the YCbCrInput test of the same name.
TEST(YCbCr422InterleavedInputTest, DifferentCbAndCrPositioning) {
	const int width = 4;
	const int height = 4;

	unsigned char uyvy[width * height * 2] = {
		/*U=*/ 64, /*Y=*/126, /*V=*/ 48, /*Y=*/126,  /*U=*/128, /*Y=*/126, /*V=*/128, /*Y=*/126,
		/*U=*/128, /*Y=*/126, /*V=*/128, /*Y=*/126,  /*U=*/192, /*Y=*/126, /*V=*/208, /*Y=*/126,
		/*U=*/128, /*Y=*/126, /*V=*/128, /*Y=*/126,  /*U=*/128, /*Y=*/126, /*V=*/128, /*Y=*/126,
		/*U=*/128, /*Y=*/126, /*V=*/128, /*Y=*/126,  /*U=*/128, /*Y=*/126, /*V=*/128, /*Y=*/126,
	};

	// Chroma samples in this case are always co-sited with a luma sample;
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

	EffectChainTester tester(nullptr, width, height);

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

	YCbCr422InterleavedInput *input = new YCbCr422InterleavedInput(format, ycbcr_format, width, height);
	input->set_pixel_data(uyvy);
	tester.get_chain()->add_input(input);

	// Y'CbCr isn't 100% accurate (the input values are rounded),
	// so we need some leeway.
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_sRGB);
	expect_equal(expected_data_red, out_data, width, height, 0.02, 0.002);

	tester.run(out_data, GL_BLUE, COLORSPACE_sRGB, GAMMA_sRGB);
	expect_equal(expected_data_blue, out_data, width, height, 0.01, 0.001);
}

TEST(YCbCr422InterleavedInputTest, PBO) {
	const int width = 2;
	const int height = 5;

	// Pure-color test inputs, calculated with the formulas in Rec. 601
        // section 2.5.4.
	unsigned char uyvy[width * height * 2] = {
		/*U=*/128, /*Y=*/ 16, /*V=*/128, /*Y=*/ 16,
		/*U=*/128, /*Y=*/235, /*V=*/128, /*Y=*/235,
		/*U=*/ 90, /*Y=*/ 81, /*V=*/240, /*Y=*/ 81,
		/*U=*/ 54, /*Y=*/145, /*V=*/ 34, /*Y=*/145,
		/*U=*/240, /*Y=*/ 41, /*V=*/110, /*Y=*/ 41,
	};

	float expected_data[4 * width * height] = {
		0.0, 0.0, 0.0, 1.0,   0.0, 0.0, 0.0, 1.0,
		1.0, 1.0, 1.0, 1.0,   1.0, 1.0, 1.0, 1.0,
		1.0, 0.0, 0.0, 1.0,   1.0, 0.0, 0.0, 1.0,
		0.0, 1.0, 0.0, 1.0,   0.0, 1.0, 0.0, 1.0,
		0.0, 0.0, 1.0, 1.0,   0.0, 0.0, 1.0, 1.0,
	};
	float out_data[4 * width * height];

	GLuint pbo;
	glGenBuffers(1, &pbo);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, pbo);
	glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, width * height * 2, uyvy, GL_STREAM_DRAW);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);

	EffectChainTester tester(nullptr, width, height);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_sRGB;

	YCbCrFormat ycbcr_format;
	ycbcr_format.luma_coefficients = YCBCR_REC_601;
	ycbcr_format.full_range = false;
	ycbcr_format.num_levels = 256;
	ycbcr_format.chroma_subsampling_x = 2;
	ycbcr_format.chroma_subsampling_y = 1;
	ycbcr_format.cb_x_position = 0.0f;  // Doesn't really matter here, since Y is constant.
	ycbcr_format.cb_y_position = 0.5f;
	ycbcr_format.cr_x_position = 0.0f;
	ycbcr_format.cr_y_position = 0.5f;

	YCbCr422InterleavedInput *input = new YCbCr422InterleavedInput(format, ycbcr_format, width, height);
	input->set_pixel_data((unsigned char *)BUFFER_OFFSET(0), pbo);
	tester.get_chain()->add_input(input);

	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_sRGB);

        // Y'CbCr isn't 100% accurate (the input values are rounded),
        // so we need some leeway.
        expect_equal(expected_data, out_data, 4 * width, height, 0.025, 0.002);

	glDeleteBuffers(1, &pbo);
}

}  // namespace movit
