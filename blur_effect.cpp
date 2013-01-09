#include <math.h>
#include <assert.h>
#include <GL/glew.h>

#include "blur_effect.h"
#include "effect_chain.h"
#include "util.h"

// Must match blur_effect.frag.
#define NUM_TAPS 16
	
BlurEffect::BlurEffect()
	: radius(3.0f),
	  input_width(1280),
	  input_height(720)
{
	// The first blur pass will forward resolution information to us.
	hpass = new SingleBlurPassEffect(this);
	CHECK(hpass->set_int("direction", SingleBlurPassEffect::HORIZONTAL));
	vpass = new SingleBlurPassEffect(NULL);
	CHECK(vpass->set_int("direction", SingleBlurPassEffect::VERTICAL));

	update_radius();
}

void BlurEffect::rewrite_graph(EffectChain *graph, Node *self)
{
	Node *hpass_node = graph->add_node(hpass);
	Node *vpass_node = graph->add_node(vpass);
	graph->connect_nodes(hpass_node, vpass_node);
	graph->replace_receiver(self, hpass_node);
	graph->replace_sender(self, vpass_node);
	self->disabled = true;
} 

// We get this information forwarded from the first blur pass,
// since we are not part of the chain ourselves.
void BlurEffect::inform_input_size(unsigned input_num, unsigned width, unsigned height)
{
	assert(input_num == 0);
	assert(width != 0);
	assert(height != 0);
	input_width = width;
	input_height = height;
	update_radius();
}
		
void BlurEffect::update_radius()
{
	// We only have 16 taps to work with on each side, and we want that to
	// reach out to about 2.5*sigma. Bump up the mipmap levels (giving us
	// box blurs) until we have what we need.
	unsigned mipmap_width = input_width, mipmap_height = input_height;
	float adjusted_radius = radius;
	while ((mipmap_width > 1 || mipmap_height > 1) && adjusted_radius * 1.5f > NUM_TAPS / 2) {
		// Find the next mipmap size (round down, minimum 1 pixel).
		mipmap_width = std::max(mipmap_width / 2, 1u);
		mipmap_height = std::max(mipmap_height / 2, 1u);

		// Approximate when mipmap sizes are odd, but good enough.
		adjusted_radius = radius * float(mipmap_width) / float(input_width);
	}
	
	bool ok = hpass->set_float("radius", adjusted_radius);
	ok |= hpass->set_int("width", mipmap_width);
	ok |= hpass->set_int("height", mipmap_height);

	ok |= vpass->set_float("radius", adjusted_radius);
	ok |= vpass->set_int("width", mipmap_width);
	ok |= vpass->set_int("height", mipmap_height);

	assert(ok);
}

bool BlurEffect::set_float(const std::string &key, float value) {
	if (key == "radius") {
		radius = value;
		update_radius();
		return true;
	}
	return false;
}

SingleBlurPassEffect::SingleBlurPassEffect(BlurEffect *parent)
	: parent(parent),
	  radius(3.0f),
	  direction(HORIZONTAL),
 	  width(1280),
 	  height(720)
{
	register_float("radius", &radius);
	register_int("direction", (int *)&direction);
	register_int("width", &width);
	register_int("height", &height);
}

std::string SingleBlurPassEffect::output_fragment_shader()
{
	return read_file("blur_effect.frag");
}

void SingleBlurPassEffect::set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num)
{
	Effect::set_gl_state(glsl_program_num, prefix, sampler_num);

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
			// Gaussian blur is a common, but maybe not the prettiest choice;
			// it can feel a bit too blurry in the fine detail and too little
			// long-tail. This is a simple logistic distribution, which has
			// a narrower peak but longer tails.
			//
			// We interpret the radius as sigma, similar to Gaussian blur.
			// Wikipedia says that sigma² = pi² s² / 3, which yields:
			const float s = (sqrt(3.0) / M_PI) * radius;
			float z = i / (2.0 * s);

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
		combine_two_samples(w1, w2, &offset, &total_weight, NULL);

		float x = 0.0f, y = 0.0f;

		if (direction == HORIZONTAL) {
			x = (base_pos + offset) / (float)width;
		} else if (direction == VERTICAL) {
			y = (base_pos + offset) / (float)height;
		} else {
			assert(false);
		}

		samples[4 * i + 0] = x;
		samples[4 * i + 1] = y;
		samples[4 * i + 2] = total_weight;
		samples[4 * i + 3] = 0.0f;
	}

	set_uniform_vec4_array(glsl_program_num, prefix, "samples", samples, NUM_TAPS / 2 + 1);
}

void SingleBlurPassEffect::clear_gl_state()
{
}
