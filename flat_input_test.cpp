// Unit tests for FlatInput.

#include <epoxy/gl.h>
#include <stddef.h>

#include "effect_chain.h"
#include "flat_input.h"
#include "gtest/gtest.h"
#include "resource_pool.h"
#include "test_util.h"
#include "util.h"

using namespace std;

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

        EffectChainTester tester(nullptr, 1, size);
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

	EffectChainTester tester(nullptr, width, height);

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

	EffectChainTester tester(nullptr, width, height);

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

	EffectChainTester tester(nullptr, width, height);

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

TEST(FlatInput, ExternalTexture) {
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

	EffectChainTester tester(nullptr, 1, size, FORMAT_RGB, COLORSPACE_sRGB, GAMMA_LINEAR);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	ResourcePool pool;
	GLuint tex = pool.create_2d_texture(GL_RGB8, 1, size);
	check_error();
	glBindTexture(GL_TEXTURE_2D, tex);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	check_error();
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	check_error();
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, size, GL_RGB, GL_FLOAT, data);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	check_error();

	FlatInput *input = new FlatInput(format, FORMAT_RGB, GL_FLOAT, 1, size);
	input->set_texture_num(tex);
	tester.get_chain()->add_input(input);

	tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR);

	pool.release_2d_texture(tex);

	expect_equal(expected_data, out_data, 4, size);
}

// Just an IdentityEffect, but marks as needing mipmaps, so that we can use it
// for downscaling to verify mipmaps were used.
class MipmapNeedingEffect : public Effect {
public:
        MipmapNeedingEffect() {}
        MipmapRequirements needs_mipmaps() const override { return NEEDS_MIPMAPS; }

        string effect_type_id() const override { return "MipmapNeedingEffect"; }
        string output_fragment_shader() override { return read_file("identity.frag"); }
};

TEST(FlatInput, ExternalTextureMipmapState) {
	const int width = 4;
	const int height = 4;

	float data[width * height] = {
		1.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
	};
	float expected_data[] = {
		0.0625,
	};
	float out_data[1];

	EffectChainTester tester(nullptr, 1, 1, FORMAT_RGB, COLORSPACE_sRGB, GAMMA_LINEAR);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	ResourcePool pool;
	GLuint tex = pool.create_2d_texture(GL_R8, width, height);
	check_error();
	glBindTexture(GL_TEXTURE_2D, tex);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
	check_error();
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	check_error();
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_FLOAT, data);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	check_error();
	glGenerateMipmap(GL_TEXTURE_2D);
	check_error();

	// Turn off mipmaps, so that we verify that Movit turns it back on.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	check_error();

	FlatInput *input = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, width, height);
	input->set_texture_num(tex);
	tester.get_chain()->add_input(input);
	tester.get_chain()->add_effect(new MipmapNeedingEffect);

	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	pool.release_2d_texture(tex);

	expect_equal(expected_data, out_data, 1, 1);
}

TEST(FlatInput, NoData) {
	const int width = 2;
	const int height = 4;

	float out_data[width * height];

	EffectChainTester tester(nullptr, width, height);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	FlatInput *input = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, width, height);
	tester.get_chain()->add_input(input);

	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	// Don't care what the output was, just that it does not crash.
}

}  // namespace movit
