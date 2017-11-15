#include <epoxy/gl.h>
#include <assert.h>
#include <math.h>
#include <algorithm>

#include "blur_effect.h"
#include "effect_chain.h"
#include "effect_util.h"
#include "init.h"
#include "util.h"

using namespace std;

namespace movit {
	
BlurEffect::BlurEffect()
	: num_taps(16),
	  radius(3.0f),
	  input_width(1280),
	  input_height(720)
{
	// The first blur pass will forward resolution information to us.
	hpass = new SingleBlurPassEffect(this);
	CHECK(hpass->set_int("direction", SingleBlurPassEffect::HORIZONTAL));
	vpass = new SingleBlurPassEffect(nullptr);
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
	while ((mipmap_width > 1 || mipmap_height > 1) && adjusted_radius * 1.5f > num_taps / 2) {
		// Find the next mipmap size (round down, minimum 1 pixel).
		mipmap_width = max(mipmap_width / 2, 1u);
		mipmap_height = max(mipmap_height / 2, 1u);

		// Approximate when mipmap sizes are odd, but good enough.
		adjusted_radius = radius * float(mipmap_width) / float(input_width);
	}
	
	bool ok = hpass->set_float("radius", adjusted_radius);
	ok |= hpass->set_int("width", mipmap_width);
	ok |= hpass->set_int("height", mipmap_height);
	ok |= hpass->set_int("virtual_width", mipmap_width);
	ok |= hpass->set_int("virtual_height", mipmap_height);
	ok |= hpass->set_int("num_taps", num_taps);

	ok |= vpass->set_float("radius", adjusted_radius);
	ok |= vpass->set_int("width", mipmap_width);
	ok |= vpass->set_int("height", mipmap_height);
	ok |= vpass->set_int("virtual_width", input_width);
	ok |= vpass->set_int("virtual_height", input_height);
	ok |= vpass->set_int("num_taps", num_taps);

	assert(ok);
}

bool BlurEffect::set_float(const string &key, float value) {
	if (key == "radius") {
		radius = value;
		update_radius();
		return true;
	}
	return false;
}

bool BlurEffect::set_int(const string &key, int value) {
	if (key == "num_taps") {
		if (value < 2 || value % 2 != 0) {
			return false;
		}
		num_taps = value;
		update_radius();
		return true;
	}
	return false;
}

SingleBlurPassEffect::SingleBlurPassEffect(BlurEffect *parent)
	: parent(parent),
	  num_taps(16),
	  radius(3.0f),
	  direction(HORIZONTAL),
	  width(1280),
	  height(720),
	  uniform_samples(nullptr)
{
	register_float("radius", &radius);
	register_int("direction", (int *)&direction);
	register_int("width", &width);
	register_int("height", &height);
	register_int("virtual_width", &virtual_width);
	register_int("virtual_height", &virtual_height);
	register_int("num_taps", &num_taps);
}

SingleBlurPassEffect::~SingleBlurPassEffect()
{
	delete[] uniform_samples;
}

string SingleBlurPassEffect::output_fragment_shader()
{
	char buf[256];
	sprintf(buf, "#define DIRECTION_VERTICAL %d\n#define NUM_TAPS %d\n",
		(direction == VERTICAL), num_taps);
	uniform_samples = new float[2 * (num_taps / 2 + 1)];
	register_uniform_vec2_array("samples", uniform_samples, num_taps / 2 + 1);
	return buf + read_file("blur_effect.frag");
}

void SingleBlurPassEffect::set_gl_state(GLuint glsl_program_num, const string &prefix, unsigned *sampler_num)
{
	Effect::set_gl_state(glsl_program_num, prefix, sampler_num);

	// Compute the weights; they will be symmetrical, so we only compute
	// the right side.
	float* weight = new float[num_taps + 1];
	if (radius < 1e-3) {
		weight[0] = 1.0f;
		for (int i = 1; i < num_taps + 1; ++i) {
			weight[i] = 0.0f;
		}
	} else {
		float sum = 0.0f;
		for (int i = 0; i < num_taps + 1; ++i) {
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
		for (int i = 0; i < num_taps + 1; ++i) {
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

	// Center sample.
	uniform_samples[2 * 0 + 0] = 0.0f;
	uniform_samples[2 * 0 + 1] = weight[0];

	int size;
	if (direction == HORIZONTAL) {
		size = width;
	} else if (direction == VERTICAL) {
		size = height;
	} else {
		assert(false);
	}
	float num_subtexels = size / movit_texel_subpixel_precision;
	float inv_num_subtexels = movit_texel_subpixel_precision / size;

	// All other samples.
	for (int i = 1; i < num_taps / 2 + 1; ++i) {
		unsigned base_pos = i * 2 - 1;
		float w1 = weight[base_pos];
		float w2 = weight[base_pos + 1];

		float pos1 = base_pos / (float)size;
		float pos, total_weight;
		combine_two_samples(w1, w2, pos1, 1.0 / (float)size, size, num_subtexels, inv_num_subtexels, &pos, &total_weight, nullptr);

		uniform_samples[2 * i + 0] = pos;
		uniform_samples[2 * i + 1] = total_weight;
	}

	delete[] weight;
}

void SingleBlurPassEffect::clear_gl_state()
{
}

}  // namespace movit
