#define GL_GLEXT_PROTOTYPES 1

#include <math.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <assert.h>

#include "blur_effect.h"
#include "util.h"

// Must match blur_effect.frag.
#define NUM_TAPS 16
	
BlurEffect::BlurEffect() {
	hpass = new SingleBlurPassEffect();
	hpass->set_int("direction", SingleBlurPassEffect::HORIZONTAL);
	vpass = new SingleBlurPassEffect();
	vpass->set_int("direction", SingleBlurPassEffect::VERTICAL);
}

void BlurEffect::add_self_to_effect_chain(std::vector<Effect *> *chain) {
	hpass->add_self_to_effect_chain(chain);
	vpass->add_self_to_effect_chain(chain);
}

bool BlurEffect::set_float(const std::string &key, float value) {
	if (!hpass->set_float(key, value)) {
		return false;
	}
	return vpass->set_float(key, value);
}

SingleBlurPassEffect::SingleBlurPassEffect()
	: radius(3.0f),
	  direction(HORIZONTAL)
{
	register_float("radius", (float *)&radius);
	register_int("direction", (int *)&direction);
}

std::string SingleBlurPassEffect::output_fragment_shader()
{
	return read_file("blur_effect.frag");
}

void SingleBlurPassEffect::set_uniforms(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num)
{
	Effect::set_uniforms(glsl_program_num, prefix, sampler_num);

	int base_texture_size, texture_size;
	if (direction == HORIZONTAL) {
		base_texture_size = texture_size = 1280;  // FIXME
	} else if (direction == VERTICAL) {
		base_texture_size = texture_size = 720;  // FIXME
	} else {
		assert(false);
	}

	// We only have 16 taps to work with on each side, and we want that to
	// reach out to about 2.5*sigma.  Bump up the mipmap levels (giving us
	// box blurs) until we have what we need.
	//
	// FIXME: we really need to pick the same mipmap level for both horizontal and vertical!
	unsigned base_mipmap_level = 0;
	float adjusted_radius = radius;
	while (texture_size > 1 && adjusted_radius * 2.5f > NUM_TAPS / 2) {
		++base_mipmap_level;
		texture_size /= 2;  // Rounding down.
		adjusted_radius = radius * float(texture_size) / float(base_texture_size);
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
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, base_mipmap_level);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, base_mipmap_level);
	check_error();

	// Compute the weights; they will be symmetrical, so we only compute
	// the right side.
	float weight[NUM_TAPS + 1];
	if (radius < 1e-3) {
		weight[0] = 1.0f;
		for (unsigned i = 1; i < NUM_TAPS + 1; ++i) {
			weight[i] = 0.0f;
		}
	} else {
		float sum = 0.0f;
		for (unsigned i = 0; i < NUM_TAPS + 1; ++i) {
			float z = i / adjusted_radius;

			// Gaussian blur is a common, but maybe not the prettiest choice;
			// it can feel a bit too blurry in the fine detail and too little
			// long-tail. This is a simple logistic distribution, which has
			// a narrower peak but longer tails.
			weight[i] = 1.0f / (cosh(z) * cosh(z));

			if (i == 0) {
				sum += weight[i];
			} else {
				sum += 2.0f * weight[i];
			}
		}
		for (unsigned i = 0; i < NUM_TAPS + 1; ++i) {
			weight[i] /= sum;
		}
	}

#if 0
	// NOTE: This is currently broken.

	// Since the GPU gives us bilinear sampling for free, we can get two
	// samples for the price of one (for every but the center sample,
	// in which case this trick doesn't buy us anything). Simply sample
	// between the two pixel centers, and we can do with fewer weights.
	// (This is right even in the vertical pass where we don't actually
	// sample between the pixels, because we have linear interpolation
	// there too.)
	//
	// We pack the parameters into a float4: The relative sample coordinates
	// in (x,y), and the weight in z. w is unused.
	float samples[4 * (NUM_TAPS / 2 + 1)];

	// Center sample.
	samples[4 * 0 + 0] = 0.0f;
	samples[4 * 0 + 1] = 0.0f;
	samples[4 * 0 + 2] = weight[0];
	samples[4 * 0 + 3] = 0.0f;

	// All other samples.
	for (unsigned i = 1; i < NUM_TAPS / 2 + 1; ++i) {
		unsigned base_pos = i * 2 - 1;
		float w1 = weight[base_pos];
		float w2 = weight[base_pos + 1];

		float offset, total_weight;
		if (w1 + w2 < 1e-6) {
			offset = 0.5f;
			total_weight = 0.0f;
		} else {
			offset = w2 / (w1 + w2);
			total_weight = w1 + w2;
		}
#if 0
		// hack for easier visualization
		offset = 0.5f;
		total_weight = 8.0f;
#endif
		float x = 0.0f, y = 0.0f;

		if (direction == HORIZONTAL) {
			x = (base_pos + offset) / (float)texture_size;
		} else if (direction == VERTICAL) {
			y = (base_pos + offset) / (float)texture_size;
		} else {
			assert(false);
		}

		samples[4 * i + 0] = x;
		samples[4 * i + 1] = y;
		samples[4 * i + 2] = total_weight;
		samples[4 * i + 3] = 0.0f;
	}

	set_uniform_vec4_array(glsl_program_num, prefix, "samples", samples, NUM_TAPS / 2 + 1);
#else
	// Boring, at-whole-pixels sampling.
	float samples[4 * NUM_TAPS];

	// All other samples.
	for (unsigned i = 0; i < NUM_TAPS + 1; ++i) {
		float x = 0.0f, y = 0.0f;

		if (direction == HORIZONTAL) {
			x = i / (float)texture_size;
		} else if (direction == VERTICAL) {
			y = i / (float)texture_size;
		} else {
			assert(false);
		}

		samples[4 * i + 0] = x;
		samples[4 * i + 1] = y;
		samples[4 * i + 2] = weight[i];
		samples[4 * i + 3] = 0.0f;
	}

	set_uniform_vec4_array(glsl_program_num, prefix, "samples", samples, NUM_TAPS + 1);
#endif
}
