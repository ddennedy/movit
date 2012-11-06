#include <math.h>
#include <assert.h>

#include "dither_effect.h"
#include "util.h"
#include "opengl.h"

namespace {

// A simple LCG (linear congruental generator) random generator.
// We implement our own so we can be deterministic from frame to frame
// and run to run; we don't have special needs for speed or quality,
// as long as the period is reasonably long. The output is in range
// [0, 2^31>.
//
// This comes from http://en.wikipedia.org/wiki/Linear_congruential_generator.
unsigned lcg_rand(unsigned x)
{
	return (x * 1103515245U + 12345U) & ((1U << 31) - 1);
} 

}  // namespace

DitherEffect::DitherEffect()
	: width(1280), height(720), num_bits(8),
	  last_width(-1), last_height(-1), last_num_bits(-1)
{
	register_int("output_width", &width);
	register_int("output_height", &height);
	register_int("num_bits", &num_bits);

	glGenTextures(1, &texnum);
}

DitherEffect::~DitherEffect()
{
	glDeleteTextures(1, &texnum);
}

std::string DitherEffect::output_fragment_shader()
{
	return read_file("dither_effect.frag");
}

void DitherEffect::update_texture(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num)
{
	float *dither_noise = new float[width * height];
	float dither_double_amplitude = 1.0f / (1 << num_bits);

	// Using the resolution as a seed gives us a consistent dither from frame to frame.
	// It also gives a different dither for e.g. different aspect ratios, which _feels_
	// good, but probably shouldn't matter.
	unsigned seed = (width << 16) ^ height;
	for (int i = 0; i < width * height; ++i) {
		seed = lcg_rand(seed);
		float normalized_rand = seed * (1.0f / (1U << 31)) - 0.5;  // [-0.5, 0.5>
		dither_noise[i] = dither_double_amplitude * normalized_rand;
	}

	glActiveTexture(GL_TEXTURE0 + *sampler_num);
	check_error();
	glBindTexture(GL_TEXTURE_2D, texnum);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	check_error();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE16F_ARB, width, height, 0, GL_LUMINANCE, GL_FLOAT, dither_noise);
	check_error();

	delete[] dither_noise;
}

void DitherEffect::set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num)
{
	Effect::set_gl_state(glsl_program_num, prefix, sampler_num);

	if (width != last_width || height != last_height || num_bits != last_num_bits) {
		update_texture(glsl_program_num, prefix, sampler_num);
		last_width = width;
		last_height = height;
		last_num_bits = num_bits;
	}

	glActiveTexture(GL_TEXTURE0 + *sampler_num);
	check_error();
	glBindTexture(GL_TEXTURE_2D, texnum);
	check_error();

	set_uniform_int(glsl_program_num, prefix, "dither_tex", *sampler_num);
	++sampler_num;
}
