#include <epoxy/gl.h>
#include <assert.h>
#include <stdio.h>
#include <algorithm>

#include "dither_effect.h"
#include "effect_util.h"
#include "init.h"
#include "util.h"

using namespace std;

namespace movit {

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
	register_uniform_float("round_fac", &uniform_round_fac);
	register_uniform_float("inv_round_fac", &uniform_inv_round_fac);
	register_uniform_vec2("tc_scale", uniform_tc_scale);
	register_uniform_sampler2d("dither_tex", &uniform_dither_tex);

	glGenTextures(1, &texnum);
}

DitherEffect::~DitherEffect()
{
	glDeleteTextures(1, &texnum);
}

string DitherEffect::output_fragment_shader()
{
	char buf[256];
	sprintf(buf, "#define NEED_EXPLICIT_ROUND %d\n", (movit_num_wrongly_rounded > 0));
	return buf + read_file("dither_effect.frag");
}

void DitherEffect::update_texture(GLuint glsl_program_num, const string &prefix, unsigned *sampler_num)
{
	float *dither_noise = new float[width * height];
	float dither_double_amplitude = 1.0f / (1 << num_bits);

	// We don't need a strictly nonrepeating dither; reducing the resolution
	// to max 128x128 saves a lot of texture bandwidth, without causing any
	// noticeable harm to the dither's performance.
	texture_width = min(width, 128);
	texture_height = min(height, 128);

	// Using the resolution as a seed gives us a consistent dither from frame to frame.
	// It also gives a different dither for e.g. different aspect ratios, which _feels_
	// good, but probably shouldn't matter.
	unsigned seed = (width << 16) ^ height;
	for (int i = 0; i < texture_width * texture_height; ++i) {
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
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	check_error();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, texture_width, texture_height, 0, GL_RED, GL_FLOAT, dither_noise);
	check_error();

	delete[] dither_noise;
}

void DitherEffect::set_gl_state(GLuint glsl_program_num, const string &prefix, unsigned *sampler_num)
{
	Effect::set_gl_state(glsl_program_num, prefix, sampler_num);

	assert(width > 0);
	assert(height > 0);
	assert(num_bits > 0);

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

	uniform_dither_tex = *sampler_num;
	++*sampler_num;

	// In theory, we should adjust for the texel centers that have moved here as well,
	// but since we use GL_NEAREST and we don't really care a lot what texel we sample,
	// we don't have to worry about it.	
	uniform_tc_scale[0] = float(width) / float(texture_width);
	uniform_tc_scale[1] = float(height) / float(texture_height);

	// Used if the shader needs to do explicit rounding.
	int round_fac = (1 << num_bits) - 1;
	uniform_round_fac = round_fac;
	uniform_inv_round_fac = 1.0f / round_fac;
}

}  // namespace movit
