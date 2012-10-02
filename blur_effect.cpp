#define GL_GLEXT_PROTOTYPES 1

#include <math.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <assert.h>

#include "blur_effect.h"
#include "util.h"

BlurEffect::BlurEffect()
	: radius(3.0f),
	  direction(HORIZONTAL)
{
	register_float("radius", (float *)&radius);
	register_int("direction", (int *)&direction);
}

std::string BlurEffect::output_fragment_shader()
{
	return read_file("blur_effect.frag");
}

void BlurEffect::set_uniforms(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num)
{
	Effect::set_uniforms(glsl_program_num, prefix, sampler_num);

	// We only have 15 taps to work with, and we want that to reach out to about 2.5*sigma.
	// Bump up the mipmap levels (giving us box blurs) until we have what we need.
	unsigned base_mipmap_level = 0;
	float adjusted_radius = radius;
	float pixel_size = 1.0f;
	while (adjusted_radius * 2.5f > 7.0f) {
		++base_mipmap_level;
		adjusted_radius *= 0.5f;
		pixel_size *= 2.0f;
	}	

	// In the second pass, we do the same, but don't sample from a mipmap;
	// that would re-blur the other direction in an ugly fashion, and we already
	// have the vertical box blur we need from that pass.
	//
	// TODO: We really need to present horizontal+vertical as a unit;
	// currently, there's really no guarantee vertical blur is the second pass.
	if (direction == VERTICAL) {
		base_mipmap_level = 0;
	}

	glActiveTexture(GL_TEXTURE0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, base_mipmap_level);
	check_error();

	// FIXME
	if (direction == HORIZONTAL) {
		float ps[] = { pixel_size / 1280.0f, 0.0f };
		set_uniform_vec2(glsl_program_num, prefix, "pixel_offset", ps);
	} else if (direction == VERTICAL) {
		float ps[] = { 0.0f, pixel_size / 720.0f };
		set_uniform_vec2(glsl_program_num, prefix, "pixel_offset", ps);
	} else {
		assert(false);
	}

	// Simple Gaussian weights for now.
	float weight[15], total = 0.0f;
	for (unsigned i = 0; i < 15; ++i) {
		float z = (i - 7.0f) / adjusted_radius;
		weight[i] = exp(-(z*z));
		total += weight[i];
	}
	printf("[mip level %d] ", base_mipmap_level);
	for (unsigned i = 0; i < 15; ++i) {
		weight[i] /= total;
		printf("%f ", weight[i]);
	}
	printf("\n");
	set_uniform_float_array(glsl_program_num, prefix, "weight", weight, 15);
}
