// Unit tests for FlatInput.

#include <epoxy/gl.h>
#include <stddef.h>

#include "effect_chain.h"
#include "flat_input.h"
#include "gtest/gtest.h"
#include "test_util.h"
#include "util.h"

namespace movit {

TEST(FlatInput, SimpleGrayscale) {
	const int size = 4;

	float data[size] = {
		0.0,
		0.5,
		0.7,
		1.0,
	};
	float expected_data[4 * size] = {
		0.0, 0.0, 0.0, 1.0,
		0.5, 0.5, 0.5, 1.0,
		0.7, 0.7, 0.7, 1.0,
		1.0, 1.0, 1.0, 1.0,
	};
	float out_data[4 * size];

	EffectChainTester tester(data, 1, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 4, size);
}

TEST(FlatInput, RGB) {
	const int size = 5;

	float data[3 * size] = {
		0.0, 0.0, 0.0,
		0.5, 0.0, 0.0,
		0.0, 0.5, 0.0,
		0.0, 0.0, 0.7,
		0.0, 0.3, 0.7,
	};
	float expected_data[4 * size] = {
		0.0, 0.0, 0.0, 1.0,
		0.5, 0.0, 0.0, 1.0,
		0.0, 0.5, 0.0, 1.0,
		0.0, 0.0, 0.7, 1.0,
		0.0, 0.3, 0.7, 1.0,
	};
	float out_data[4 * size];

	EffectChainTester tester(data, 1, size, FORMAT_RGB, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 4, size);
}

TEST(FlatInput, RGBA) {
	const int size = 5;

	float data[4 * size] = {
		0.0, 0.0, 0.0, 1.0,
		0.5, 0.0, 0.0, 0.3,
		0.0, 0.5, 0.0, 0.7,
		0.0, 0.0, 0.7, 1.0,
		0.0, 0.3, 0.7, 0.2,
	};
	float expected_data[4 * size] = {
		0.0, 0.0, 0.0, 1.0,
		0.5, 0.0, 0.0, 0.3,
		0.0, 0.5, 0.0, 0.7,
		0.0, 0.0, 0.7, 1.0,
		0.0, 0.3, 0.7, 0.2,
	};
	float out_data[4 * size];

	EffectChainTester tester(data, 1, size, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 4, size);
}

// Note: The sRGB conversion itself is tested in EffectChainTester,
// since it also wants to test the chain building itself.
// Here, we merely test that alpha is left alone; the test will usually
// run using the sRGB OpenGL extension, but might be run with a
// GammaExpansionEffect if the card/driver happens not to support that.
TEST(FlatInput, AlphaIsNotModifiedBySRGBConversion) {
	const int size = 5;

	unsigned char data[4 * size] = {
		0, 0, 0, 0,
		0, 0, 0, 63,
		0, 0, 0, 127,
		0, 0, 0, 191,
		0, 0, 0, 255,
	};
	float expected_data[4 * size] = {
		0, 0, 0, 0.0 / 255.0,
		0, 0, 0, 63.0 / 255.0,
		0, 0, 0, 127.0 / 255.0,
		0, 0, 0, 191.0 / 255.0,
		0, 0, 0, 255.0 / 255.0,
	};
	float out_data[4 * size];

        EffectChainTester tester(NULL, 1, size);
        tester.add_input(data, FORMAT_RGBA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_sRGB);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 4, size);
}

TEST(FlatInput, BGR) {
	const int size = 5;

	float data[3 * size] = {
		0.0, 0.0, 0.0,
		0.5, 0.0, 0.0,
		0.0, 0.5, 0.0,
		0.0, 0.0, 0.7,
		0.0, 0.3, 0.7,
	};
	float expected_data[4 * size] = {
		0.0, 0.0, 0.0, 1.0,
		0.0, 0.0, 0.5, 1.0,
		0.0, 0.5, 0.0, 1.0,
		0.7, 0.0, 0.0, 1.0,
		0.7, 0.3, 0.0, 1.0,
	};
	float out_data[4 * size];

	EffectChainTester tester(data, 1, size, FORMAT_BGR, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 4, size);
}

TEST(FlatInput, BGRA) {
	const int size = 5;

	float data[4 * size] = {
		0.0, 0.0, 0.0, 1.0,
		0.5, 0.0, 0.0, 0.3,
		0.0, 0.5, 0.0, 0.7,
		0.0, 0.0, 0.7, 1.0,
		0.0, 0.3, 0.7, 0.2,
	};
	float expected_data[4 * size] = {
		0.0, 0.0, 0.0, 1.0,
		0.0, 0.0, 0.5, 0.3,
		0.0, 0.5, 0.0, 0.7,
		0.7, 0.0, 0.0, 1.0,
		0.7, 0.3, 0.0, 0.2,
	};
	float out_data[4 * size];

	EffectChainTester tester(data, 1, size, FORMAT_BGRA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, 4, size);
}

TEST(FlatInput, Pitch) {
	const int pitch = 3;
	const int width = 2;
	const int height = 4;

	float data[pitch * height] = {
		0.0, 1.0, 999.0f,
		0.5, 0.5, 999.0f,
		0.7, 0.2, 999.0f,
		1.0, 0.6, 999.0f,
	};
	float expected_data[4 * width * height] = {
		0.0, 0.0, 0.0, 1.0,  1.0, 1.0, 1.0, 1.0,
		0.5, 0.5, 0.5, 1.0,  0.5, 0.5, 0.5, 1.0,
		0.7, 0.7, 0.7, 1.0,  0.2, 0.2, 0.2, 1.0,
		1.0, 1.0, 1.0, 1.0,  0.6, 0.6, 0.6, 1.0,
	};
	float out_data[4 * width * height];

	EffectChainTester tester(NULL, width, height);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	FlatInput *input = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, width, height);
	input->set_pitch(pitch);
	input->set_pixel_data(data);
	tester.get_chain()->add_input(input);

	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);
	expect_equal(expected_data, out_data, 4 * width, height);
}

TEST(FlatInput, UpdatedData) {
	const int width = 2;
	const int height = 4;

	float data[width * height] = {
		0.0, 1.0,
		0.5, 0.5,
		0.7, 0.2,
		1.0, 0.6,
	};
	float out_data[width * height];

	EffectChainTester tester(NULL, width, height);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	FlatInput *input = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, width, height);
	input->set_pixel_data(data);
	tester.get_chain()->add_input(input);

	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);
	expect_equal(data, out_data, width, height);

	data[6] = 0.3;
	input->invalidate_pixel_data();

	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);
	expect_equal(data, out_data, width, height);
}

TEST(FlatInput, PBO) {
	const int width = 3;
	const int height = 2;

	float data[width * height] = {
		0.0, 1.0, 0.5,
		0.5, 0.5, 0.2,
	};
	float expected_data[4 * width * height] = {
		0.0, 0.0, 0.0, 1.0,  1.0, 1.0, 1.0, 1.0,  0.5, 0.5, 0.5, 1.0,
		0.5, 0.5, 0.5, 1.0,  0.5, 0.5, 0.5, 1.0,  0.2, 0.2, 0.2, 1.0,
	};
	float out_data[4 * width * height];

	GLuint pbo;
	glGenBuffers(1, &pbo);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, pbo);
	glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, width * height * sizeof(float), data, GL_STREAM_DRAW);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);

	EffectChainTester tester(NULL, width, height);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	FlatInput *input = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, width, height);
	input->set_pixel_data((float *)BUFFER_OFFSET(0), pbo);
	tester.get_chain()->add_input(input);

	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);
	expect_equal(expected_data, out_data, 4 * width, height);

	glDeleteBuffers(1, &pbo);
}

}  // namespace movit
