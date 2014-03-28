#include <epoxy/gl.h>
#include <string.h>

#include "complex_modulate_effect.h"
#include "effect_chain.h"
#include "fft_convolution_effect.h"
#include "fft_input.h"
#include "fft_pass_effect.h"
#include "multiply_effect.h"
#include "padding_effect.h"
#include "slice_effect.h"
#include "util.h"

using namespace std;

namespace movit {

FFTConvolutionEffect::FFTConvolutionEffect(int input_width, int input_height, int convolve_width, int convolve_height)
	: input_width(input_width),
	  input_height(input_height),
	  convolve_width(convolve_width),
	  convolve_height(convolve_height),
	  fft_input(new FFTInput(convolve_width, convolve_height)),
	  crop_effect(new PaddingEffect()),
	  owns_effects(true) {
	CHECK(crop_effect->set_int("width", input_width));
	CHECK(crop_effect->set_int("height", input_height));
	CHECK(crop_effect->set_float("top", 0));
	CHECK(crop_effect->set_float("left", 0));
}

FFTConvolutionEffect::~FFTConvolutionEffect()
{
	if (owns_effects) {
		delete fft_input;
		delete crop_effect;
	}
}

namespace {

// Returns the last Effect in the new chain.
Effect *add_overlap_and_fft(EffectChain *chain, Effect *last_effect, int fft_size, int pad_size, FFTPassEffect::Direction direction)
{
	// Overlap.
	{
		Effect *overlap_effect = chain->add_effect(new SliceEffect(), last_effect);
		CHECK(overlap_effect->set_int("input_slice_size", fft_size - pad_size));
		CHECK(overlap_effect->set_int("output_slice_size", fft_size));
		CHECK(overlap_effect->set_int("offset", -pad_size));
		if (direction == FFTPassEffect::HORIZONTAL) {
			CHECK(overlap_effect->set_int("direction", SliceEffect::HORIZONTAL));
		} else {
			assert(direction == FFTPassEffect::VERTICAL);
			CHECK(overlap_effect->set_int("direction", SliceEffect::VERTICAL));
		}

		last_effect = overlap_effect;
	}

	// FFT.
	int num_passes = ffs(fft_size) - 1;
	for (int i = 1; i <= num_passes; ++i) {
		Effect *fft_effect = chain->add_effect(new FFTPassEffect(), last_effect);
		CHECK(fft_effect->set_int("pass_number", i));
		CHECK(fft_effect->set_int("fft_size", fft_size));
		CHECK(fft_effect->set_int("direction", direction));
		CHECK(fft_effect->set_int("inverse", 0));

		last_effect = fft_effect;
	}

	return last_effect;
}

// Returns the last Effect in the new chain.
Effect *add_ifft_and_discard(EffectChain *chain, Effect *last_effect, int fft_size, int pad_size, FFTPassEffect::Direction direction)
{
	// IFFT.
	int num_passes = ffs(fft_size) - 1;
	for (int i = 1; i <= num_passes; ++i) {
		Effect *fft_effect = chain->add_effect(new FFTPassEffect(), last_effect);
		CHECK(fft_effect->set_int("pass_number", i));
		CHECK(fft_effect->set_int("fft_size", fft_size));
		CHECK(fft_effect->set_int("direction", direction));
		CHECK(fft_effect->set_int("inverse", 1));

		last_effect = fft_effect;
	}

	// Discard.
	{
		Effect *discard_effect = chain->add_effect(new SliceEffect(), last_effect);
		CHECK(discard_effect->set_int("input_slice_size", fft_size));
		CHECK(discard_effect->set_int("output_slice_size", fft_size - pad_size));
		if (direction == FFTPassEffect::HORIZONTAL) {
			CHECK(discard_effect->set_int("direction", SliceEffect::HORIZONTAL));
		} else {
			assert(direction == FFTPassEffect::VERTICAL);
			CHECK(discard_effect->set_int("direction", SliceEffect::VERTICAL));
		}
		CHECK(discard_effect->set_int("offset", pad_size));

		last_effect = discard_effect;
	}

	return last_effect;
}

}  // namespace

void FFTConvolutionEffect::rewrite_graph(EffectChain *chain, Node *self)
{
	int pad_width = convolve_width - 1;
	int pad_height = convolve_height - 1;

	// Try all possible FFT widths and heights to see which one is the
	// cheapest.  As a proxy for real performance, we use number of texel
	// fetches; this isn't perfect by any means, but it's easy to work with
	// and should be approximately correct.
	int min_x = next_power_of_two(1 + pad_width);
	int min_y = next_power_of_two(1 + pad_height);
	int max_y = next_power_of_two(input_height + pad_width);
	int max_x = next_power_of_two(input_width + pad_height);

	size_t best_cost = numeric_limits<size_t>::max();
	int best_x = -1, best_y = -1, best_x_before_y_fft = -1, best_x_before_y_ifft = -1;

	// Try both
	//
	//   overlap(X), FFT(X), overlap(Y), FFT(Y), modulate, IFFT(Y), discard(Y), IFFT(X), discard(X) and
	//   overlap(Y), FFT(Y), overlap(X), FFT(X), modulate, IFFT(X), discard(X), IFFT(Y), discard(Y)
	//
	// For simplicity, call them the XY-YX and YX-XY orders. In theory, we
	// could have XY-XY and YX-YX orders as well, and I haven't found a
	// convincing argument that they will never be optimal (although it
	// sounds odd and should be rare), so we test all four possible ones.
	//
	// We assume that the kernel FFT is for free, since it is typically done
	// only once and per frame.
	for (int x_before_y_fft = 0; x_before_y_fft <= 1; ++x_before_y_fft) {
		for (int x_before_y_ifft = 0; x_before_y_ifft <= 1; ++x_before_y_ifft) {
			for (int y = min_y; y <= max_y; y *= 2) {
				int y_pixels_per_block = y - pad_height;
				int num_vertical_blocks = div_round_up(input_height, y_pixels_per_block);
				size_t output_height = y * num_vertical_blocks;
				for (int x = min_x; x <= max_x; x *= 2) {
					int x_pixels_per_block = x - pad_width;
					int num_horizontal_blocks = div_round_up(input_width, x_pixels_per_block);
					size_t output_width = x * num_horizontal_blocks;

					size_t cost = 0;

					if (x_before_y_fft) {
						// First, the cost of the horizontal padding.
						cost = output_width * input_height;

						// log(X) FFT passes. Each pass reads two inputs per pixel,
						// plus the support texture.
						cost += (ffs(x) - 1) * 3 * output_width * input_height;

						// Now, horizontal padding.
						cost += output_width * output_height;

						// log(Y) FFT passes, now at full resolution.
						cost += (ffs(y) - 1) * 3 * output_width * output_height;
					} else {
						// First, the cost of the vertical padding.
						cost = input_width * output_height;

						// log(Y) FFT passes. Each pass reads two inputs per pixel,
						// plus the support texture.
						cost += (ffs(y) - 1) * 3 * input_width * output_height;

						// Now, horizontal padding.
						cost += output_width * output_height;

						// log(X) FFT passes, now at full resolution.
						cost += (ffs(x) - 1) * 3 * output_width * output_height;
					}

					// The actual modulation. Reads one pixel each from two textures.
					cost += 2 * output_width * output_height;

					if (x_before_y_ifft) {
						// log(X) IFFT passes.
						cost += (ffs(x) - 1) * 3 * output_width * output_height;

						// Discard horizontally.
						cost += input_width * output_height;

						// log(Y) IFFT passes.
						cost += (ffs(y) - 1) * 3 * input_width * output_height;

						// Discard horizontally.
						cost += input_width * input_height;
					} else {
						// log(Y) IFFT passes.
						cost += (ffs(y) - 1) * 3 * output_width * output_height;

						// Discard vertically.
						cost += output_width * input_height;

						// log(X) IFFT passes.
						cost += (ffs(x) - 1) * 3 * output_width * input_height;

						// Discard horizontally.
						cost += input_width * input_height;
					}

					if (cost < best_cost) {
						best_x = x;
						best_y = y;
						best_x_before_y_fft = x_before_y_fft;
						best_x_before_y_ifft = x_before_y_ifft;
						best_cost = cost;
					}
				}
			}
		}
	}

	const int fft_width = best_x, fft_height = best_y;

	assert(self->incoming_links.size() == 1);
	Node *last_node = self->incoming_links[0];
	self->incoming_links.clear();
	last_node->outgoing_links.clear();

	// Do FFT.
	Effect *last_effect = last_node->effect;
	if (best_x_before_y_fft) {
		last_effect = add_overlap_and_fft(chain, last_effect, fft_width, pad_width, FFTPassEffect::HORIZONTAL);
		last_effect = add_overlap_and_fft(chain, last_effect, fft_height, pad_height, FFTPassEffect::VERTICAL);
	} else {
		last_effect = add_overlap_and_fft(chain, last_effect, fft_height, pad_height, FFTPassEffect::VERTICAL);
		last_effect = add_overlap_and_fft(chain, last_effect, fft_width, pad_width, FFTPassEffect::HORIZONTAL);
	}

	// Normalizer.
	Effect *multiply_effect;
	float fft_size = fft_width * fft_height;
	float factor[4] = { 1.0f / fft_size, 1.0f / fft_size, 1.0f / fft_size, 1.0f / fft_size };
	last_effect = multiply_effect = chain->add_effect(new MultiplyEffect(), last_effect);
	CHECK(multiply_effect->set_vec4("factor", factor));

	// Multiply by the FFT of the convolution kernel.
	CHECK(fft_input->set_int("fft_width", fft_width));
	CHECK(fft_input->set_int("fft_height", fft_height));
	chain->add_input(fft_input);
	owns_effects = false;

	Effect *modulate_effect = chain->add_effect(new ComplexModulateEffect(), multiply_effect, fft_input);
	CHECK(modulate_effect->set_int("num_repeats_x", div_round_up(input_width, fft_width - pad_width)));
	CHECK(modulate_effect->set_int("num_repeats_y", div_round_up(input_height, fft_height - pad_height)));
	last_effect = modulate_effect;

	// Finally, do IFFT.
	if (best_x_before_y_ifft) {
		last_effect = add_ifft_and_discard(chain, last_effect, fft_width, pad_width, FFTPassEffect::HORIZONTAL);
		last_effect = add_ifft_and_discard(chain, last_effect, fft_height, pad_height, FFTPassEffect::VERTICAL);
	} else {
		last_effect = add_ifft_and_discard(chain, last_effect, fft_height, pad_height, FFTPassEffect::VERTICAL);
		last_effect = add_ifft_and_discard(chain, last_effect, fft_width, pad_width, FFTPassEffect::HORIZONTAL);
	}

	// ...and crop away any extra padding we have have added.
	last_effect = chain->add_effect(crop_effect);

	chain->replace_sender(self, chain->find_node_for_effect(last_effect));
	self->disabled = true;
}

}  // namespace movit
