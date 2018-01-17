// Three-lobed Lanczos, the most common choice.
// Note that if you change this, the accuracy for LANCZOS_TABLE_SIZE
// needs to be recomputed.
#define LANCZOS_RADIUS 3.0f

#include <epoxy/gl.h>
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <algorithm>
#include <mutex>
#include <Eigen/Sparse>
#include <Eigen/SparseQR>
#include <Eigen/OrderingMethods>

#include "effect_chain.h"
#include "effect_util.h"
#include "fp16.h"
#include "init.h"
#include "resample_effect.h"
#include "util.h"

using namespace Eigen;
using namespace std;

namespace movit {

namespace {

float sinc(float x)
{
	if (fabs(x) < 1e-6) {
		return 1.0f - fabs(x);
	} else {
		return sin(x) / x;
	}
}

float lanczos_weight(float x)
{
	if (fabs(x) > LANCZOS_RADIUS) {
		return 0.0f;
	} else {
		return sinc(M_PI * x) * sinc((M_PI / LANCZOS_RADIUS) * x);
	}
}

// The weight function can be expensive to compute over and over again
// (which will happen during e.g. a zoom), but it is also easy to interpolate
// linearly. We compute the right half of the function (in the range of
// 0..LANCZOS_RADIUS), with two guard elements for easier interpolation, and
// linearly interpolate to get our function.
//
// We want to scale the table so that the maximum error is always smaller
// than 1e-6. As per http://www-solar.mcs.st-andrews.ac.uk/~clare/Lectures/num-analysis/Numan_chap3.pdf,
// the error for interpolating a function linearly between points [a,b] is
//
//   e = 1/2 (x-a)(x-b) f''(u_x)
//
// for some point u_x in [a,b] (where f(x) is our Lanczos function; we're
// assuming LANCZOS_RADIUS=3 from here on). Obviously this is bounded by
// f''(x) over the entire range. Numeric optimization shows the maximum of
// |f''(x)| to be in x=1.09369819474562880, with the value 2.40067758733152381.
// So if the steps between consecutive values are called d, we get
//
//   |e| <= 1/2 (d/2)^2 2.4007
//   |e| <= 0.1367 d^2
//
// Solve for e = 1e-6 yields a step size of 0.0027, which to cover the range
// 0..3 needs 1109 steps. We round up to the next power of two, just to be sure.
//
// You need to call lanczos_table_init_done before the first call to
// lanczos_weight_cached.
#define LANCZOS_TABLE_SIZE 2048
static once_flag lanczos_table_init_done;
float lanczos_table[LANCZOS_TABLE_SIZE + 2];

void init_lanczos_table()
{
	for (unsigned i = 0; i < LANCZOS_TABLE_SIZE + 2; ++i) {
		lanczos_table[i] = lanczos_weight(float(i) * (LANCZOS_RADIUS / LANCZOS_TABLE_SIZE));
	}
}

float lanczos_weight_cached(float x)
{
	x = fabs(x);
	if (x > LANCZOS_RADIUS) {
		return 0.0f;
	}
	float table_pos = x * (LANCZOS_TABLE_SIZE / LANCZOS_RADIUS);
	unsigned table_pos_int = int(table_pos);  // Truncate towards zero.
	float table_pos_frac = table_pos - table_pos_int;
	assert(table_pos < LANCZOS_TABLE_SIZE + 2);
	return lanczos_table[table_pos_int] +
		table_pos_frac * (lanczos_table[table_pos_int + 1] - lanczos_table[table_pos_int]);
}

// Euclid's algorithm, from Wikipedia.
unsigned gcd(unsigned a, unsigned b)
{
	while (b != 0) {
		unsigned t = b;
		b = a % b;
		a = t;
	}
	return a;
}

template<class DestFloat>
unsigned combine_samples(const Tap<float> *src, Tap<DestFloat> *dst, float num_subtexels, float inv_num_subtexels, unsigned num_src_samples, unsigned max_samples_saved, float pos1_pos2_diff, float inv_pos1_pos2_diff)
{
	// Cut off near-zero values at both sides.
	unsigned num_samples_saved = 0;
	while (num_samples_saved < max_samples_saved &&
	       num_src_samples > 0 &&
	       fabs(src[0].weight) < 1e-6) {
		++src;
		--num_src_samples;
		++num_samples_saved;
	}
	while (num_samples_saved < max_samples_saved &&
	       num_src_samples > 0 &&
	       fabs(src[num_src_samples - 1].weight) < 1e-6) {
		--num_src_samples;
		++num_samples_saved;
	}

	for (unsigned i = 0, j = 0; i < num_src_samples; ++i, ++j) {
		// Copy the sample directly; it will be overwritten later if we can combine.
		if (dst != nullptr) {
			dst[j].weight = convert_float<float, DestFloat>(src[i].weight);
			dst[j].pos = convert_float<float, DestFloat>(src[i].pos);
		}

		if (i == num_src_samples - 1) {
			// Last sample; cannot combine.
			continue;
		}
		assert(num_samples_saved <= max_samples_saved);
		if (num_samples_saved == max_samples_saved) {
			// We could maybe save more here, but other rows can't, so don't bother.
			continue;
		}

		float w1 = src[i].weight;
		float w2 = src[i + 1].weight;
		if (w1 * w2 < 0.0f) {
			// Differing signs; cannot combine.
			continue;
		}

		float pos1 = src[i].pos;
		float pos2 = src[i + 1].pos;
		assert(pos2 > pos1);

		DestFloat pos, total_weight;
		float sum_sq_error;
		combine_two_samples(w1, w2, pos1, pos1_pos2_diff, inv_pos1_pos2_diff, num_subtexels, inv_num_subtexels, &pos, &total_weight, &sum_sq_error);

		// If the interpolation error is larger than that of about sqrt(2) of
		// a level at 8-bit precision, don't combine. (You'd think 1.0 was enough,
		// but since the artifacts are not really random, they can get quite
		// visible. On the other hand, going to 0.25f, I can see no change at
		// all with 8-bit output, so it would not seem to be worth it.)
		if (sum_sq_error > 0.5f / (255.0f * 255.0f)) {
			continue;
		}

		// OK, we can combine this and the next sample.
		if (dst != nullptr) {
			dst[j].weight = total_weight;
			dst[j].pos = pos;
		}

		++i;  // Skip the next sample.
		++num_samples_saved;
	}
	return num_samples_saved;
}

// Normalize so that the sum becomes one. Note that we do it twice;
// this sometimes helps a tiny little bit when we have many samples.
template<class T>
void normalize_sum(Tap<T>* vals, unsigned num)
{
	for (int normalize_pass = 0; normalize_pass < 2; ++normalize_pass) {
		float sum = 0.0;
		for (unsigned i = 0; i < num; ++i) {
			sum += to_fp32(vals[i].weight);
		}
		float inv_sum = 1.0 / sum;
		for (unsigned i = 0; i < num; ++i) {
			vals[i].weight = from_fp32<T>(to_fp32(vals[i].weight) * inv_sum);
		}
	}
}

// Make use of the bilinear filtering in the GPU to reduce the number of samples
// we need to make. This is a bit more complex than BlurEffect since we cannot combine
// two neighboring samples if their weights have differing signs, so we first need to
// figure out the maximum number of samples. Then, we downconvert all the weights to
// that number -- we could have gone for a variable-length system, but this is simpler,
// and the gains would probably be offset by the extra cost of checking when to stop.
//
// The greedy strategy for combining samples is optimal.
template<class DestFloat>
unsigned combine_many_samples(const Tap<float> *weights, unsigned src_size, unsigned src_samples, unsigned dst_samples, unique_ptr<Tap<DestFloat>[]> *bilinear_weights)
{
	float num_subtexels = src_size / movit_texel_subpixel_precision;
	float inv_num_subtexels = movit_texel_subpixel_precision / src_size;
	float pos1_pos2_diff = 1.0f / src_size;
	float inv_pos1_pos2_diff = src_size;

	unsigned max_samples_saved = UINT_MAX;
	for (unsigned y = 0; y < dst_samples && max_samples_saved > 0; ++y) {
		unsigned num_samples_saved = combine_samples<DestFloat>(weights + y * src_samples, nullptr, num_subtexels, inv_num_subtexels, src_samples, max_samples_saved, pos1_pos2_diff, inv_pos1_pos2_diff);
		max_samples_saved = min(max_samples_saved, num_samples_saved);
	}

	// Now that we know the right width, actually combine the samples.
	unsigned src_bilinear_samples = src_samples - max_samples_saved;
	bilinear_weights->reset(new Tap<DestFloat>[dst_samples * src_bilinear_samples]);
	for (unsigned y = 0; y < dst_samples; ++y) {
		Tap<DestFloat> *bilinear_weights_ptr = bilinear_weights->get() + y * src_bilinear_samples;
		unsigned num_samples_saved = combine_samples(
			weights + y * src_samples,
			bilinear_weights_ptr,
			num_subtexels,
			inv_num_subtexels,
			src_samples,
			max_samples_saved,
			pos1_pos2_diff,
			inv_pos1_pos2_diff);
		assert(num_samples_saved == max_samples_saved);
		normalize_sum(bilinear_weights_ptr, src_bilinear_samples);
	}
	return src_bilinear_samples;
}

// Compute the sum of squared errors between the ideal weights (which are
// assumed to fall exactly on pixel centers) and the weights that result
// from sampling at <bilinear_weights>. The primary reason for the difference
// is inaccuracy in the sampling positions, both due to limited precision
// in storing them (already inherent in sending them in as fp16_int_t)
// and in subtexel sampling precision (which we calculate in this function).
template<class T>
double compute_sum_sq_error(const Tap<float>* weights, unsigned num_weights,
                            const Tap<T>* bilinear_weights, unsigned num_bilinear_weights,
                            unsigned size)
{
	// Find the effective range of the bilinear-optimized kernel.
	// Due to rounding of the positions, this is not necessarily the same
	// as the intended range (ie., the range of the original weights).
	int lower_pos = int(floor(to_fp32(bilinear_weights[0].pos) * size - 0.5f));
	int upper_pos = int(ceil(to_fp32(bilinear_weights[num_bilinear_weights - 1].pos) * size - 0.5f)) + 2;
	lower_pos = min<int>(lower_pos, lrintf(weights[0].pos * size - 0.5f));
	upper_pos = max<int>(upper_pos, lrintf(weights[num_weights - 1].pos * size - 0.5f) + 1);

	float* effective_weights = new float[upper_pos - lower_pos];
	for (int i = 0; i < upper_pos - lower_pos; ++i) {
		effective_weights[i] = 0.0f;
	}

	// Now find the effective weights that result from this sampling.
	for (unsigned i = 0; i < num_bilinear_weights; ++i) {
		const float pixel_pos = to_fp32(bilinear_weights[i].pos) * size - 0.5f;
		const int x0 = int(floor(pixel_pos)) - lower_pos;
		const int x1 = x0 + 1;
		const float f = lrintf((pixel_pos - (x0 + lower_pos)) / movit_texel_subpixel_precision) * movit_texel_subpixel_precision;

		assert(x0 >= 0);
		assert(x1 >= 0);
		assert(x0 < upper_pos - lower_pos);
		assert(x1 < upper_pos - lower_pos);

		effective_weights[x0] += to_fp32(bilinear_weights[i].weight) * (1.0f - f);
		effective_weights[x1] += to_fp32(bilinear_weights[i].weight) * f;
	}

	// Subtract the desired weights to get the error.
	for (unsigned i = 0; i < num_weights; ++i) {
		const int x = lrintf(weights[i].pos * size - 0.5f) - lower_pos;
		assert(x >= 0);
		assert(x < upper_pos - lower_pos);

		effective_weights[x] -= weights[i].weight;
	}

	double sum_sq_error = 0.0;
	for (unsigned i = 0; i < num_weights; ++i) {
		sum_sq_error += effective_weights[i] * effective_weights[i];
	}

	delete[] effective_weights;
	return sum_sq_error;
}

}  // namespace

ResampleEffect::ResampleEffect()
	: input_width(1280),
	  input_height(720),
	  offset_x(0.0f), offset_y(0.0f),
	  zoom_x(1.0f), zoom_y(1.0f),
	  zoom_center_x(0.5f), zoom_center_y(0.5f)
{
	register_int("width", &output_width);
	register_int("height", &output_height);

	// The first blur pass will forward resolution information to us.
	hpass_owner.reset(new SingleResamplePassEffect(this));
	hpass = hpass_owner.get();
	CHECK(hpass->set_int("direction", SingleResamplePassEffect::HORIZONTAL));
	vpass_owner.reset(new SingleResamplePassEffect(this));
	vpass = vpass_owner.get();
	CHECK(vpass->set_int("direction", SingleResamplePassEffect::VERTICAL));

	update_size();
}

ResampleEffect::~ResampleEffect()
{
}

void ResampleEffect::rewrite_graph(EffectChain *graph, Node *self)
{
	Node *hpass_node = graph->add_node(hpass_owner.release());
	Node *vpass_node = graph->add_node(vpass_owner.release());
	graph->connect_nodes(hpass_node, vpass_node);
	graph->replace_receiver(self, hpass_node);
	graph->replace_sender(self, vpass_node);
	self->disabled = true;
} 

// We get this information forwarded from the first blur pass,
// since we are not part of the chain ourselves.
void ResampleEffect::inform_input_size(unsigned input_num, unsigned width, unsigned height)
{
	assert(input_num == 0);
	assert(width != 0);
	assert(height != 0);
	input_width = width;
	input_height = height;
	update_size();
}

void ResampleEffect::update_size()
{
	bool ok = true;
	ok |= hpass->set_int("input_width", input_width);
	ok |= hpass->set_int("input_height", input_height);
	ok |= hpass->set_int("output_width", output_width);
	ok |= hpass->set_int("output_height", input_height);

	ok |= vpass->set_int("input_width", output_width);
	ok |= vpass->set_int("input_height", input_height);
	ok |= vpass->set_int("output_width", output_width);
	ok |= vpass->set_int("output_height", output_height);

	assert(ok);

	// The offset added due to zoom may have changed with the size.
	update_offset_and_zoom();
}

void ResampleEffect::update_offset_and_zoom()
{
	bool ok = true;

	// Zoom from the right origin. (zoom_center is given in normalized coordinates,
	// i.e. 0..1.)
	float extra_offset_x = zoom_center_x * (1.0f - 1.0f / zoom_x) * input_width;
	float extra_offset_y = (1.0f - zoom_center_y) * (1.0f - 1.0f / zoom_y) * input_height;

	ok |= hpass->set_float("offset", extra_offset_x + offset_x);
	ok |= vpass->set_float("offset", extra_offset_y - offset_y);  // Compensate for the bottom-left origin.
	ok |= hpass->set_float("zoom", zoom_x);
	ok |= vpass->set_float("zoom", zoom_y);

	assert(ok);
}

bool ResampleEffect::set_float(const string &key, float value) {
	if (key == "width") {
		output_width = value;
		update_size();
		return true;
	}
	if (key == "height") {
		output_height = value;
		update_size();
		return true;
	}
	if (key == "top") {
		offset_y = value;
		update_offset_and_zoom();
		return true;
	}
	if (key == "left") {
		offset_x = value;
		update_offset_and_zoom();
		return true;
	}
	if (key == "zoom_x") {
		if (value <= 0.0f) {
			return false;
		}
		zoom_x = value;
		update_offset_and_zoom();
		return true;
	}
	if (key == "zoom_y") {
		if (value <= 0.0f) {
			return false;
		}
		zoom_y = value;
		update_offset_and_zoom();
		return true;
	}
	if (key == "zoom_center_x") {
		zoom_center_x = value;
		update_offset_and_zoom();
		return true;
	}
	if (key == "zoom_center_y") {
		zoom_center_y = value;
		update_offset_and_zoom();
		return true;
	}
	return false;
}

SingleResamplePassEffect::SingleResamplePassEffect(ResampleEffect *parent)
	: parent(parent),
	  direction(HORIZONTAL),
	  input_width(1280),
	  input_height(720),
	  offset(0.0),
	  zoom(1.0),
	  last_input_width(-1),
	  last_input_height(-1),
	  last_output_width(-1),
	  last_output_height(-1),
	  last_offset(0.0 / 0.0),  // NaN.
	  last_zoom(0.0 / 0.0)  // NaN.
{
	register_int("direction", (int *)&direction);
	register_int("input_width", &input_width);
	register_int("input_height", &input_height);
	register_int("output_width", &output_width);
	register_int("output_height", &output_height);
	register_float("offset", &offset);
	register_float("zoom", &zoom);
	register_uniform_sampler2d("sample_tex", &uniform_sample_tex);
	register_uniform_int("num_samples", &uniform_num_samples);
	register_uniform_float("num_loops", &uniform_num_loops);
	register_uniform_float("slice_height", &uniform_slice_height);
	register_uniform_float("sample_x_scale", &uniform_sample_x_scale);
	register_uniform_float("sample_x_offset", &uniform_sample_x_offset);
	register_uniform_float("whole_pixel_offset", &uniform_whole_pixel_offset);

	call_once(lanczos_table_init_done, init_lanczos_table);
}

SingleResamplePassEffect::~SingleResamplePassEffect()
{
}

string SingleResamplePassEffect::output_fragment_shader()
{
	char buf[256];
	sprintf(buf, "#define DIRECTION_VERTICAL %d\n", (direction == VERTICAL));
	return buf + read_file("resample_effect.frag");
}

// Using vertical scaling as an example:
//
// Generally out[y] = w0 * in[yi] + w1 * in[yi + 1] + w2 * in[yi + 2] + ...
//
// Obviously, yi will depend on y (in a not-quite-linear way), but so will
// the weights w0, w1, w2, etc.. The easiest way of doing this is to encode,
// for each sample, the weight and the yi value, e.g. <yi, w0>, <yi + 1, w1>,
// and so on. For each y, we encode these along the x-axis (since that is spare),
// so out[0] will read from parameters <x,y> = <0,0>, <1,0>, <2,0> and so on.
//
// For horizontal scaling, we fill in the exact same texture;
// the shader just interprets it differently.
void SingleResamplePassEffect::update_texture(GLuint glsl_program_num, const string &prefix, unsigned *sampler_num)
{
	unsigned src_size, dst_size;
	if (direction == SingleResamplePassEffect::HORIZONTAL) {
		assert(input_height == output_height);
		src_size = input_width;
		dst_size = output_width;
	} else if (direction == SingleResamplePassEffect::VERTICAL) {
		assert(input_width == output_width);
		src_size = input_height;
		dst_size = output_height;
	} else {
		assert(false);
	}

	ScalingWeights weights = calculate_bilinear_scaling_weights(src_size, dst_size, zoom, offset);
	src_bilinear_samples = weights.src_bilinear_samples;
	num_loops = weights.num_loops;
	slice_height = 1.0f / weights.num_loops;

	// Encode as a two-component texture. Note the GL_REPEAT.
	glActiveTexture(GL_TEXTURE0 + *sampler_num);
	check_error();
	glBindTexture(GL_TEXTURE_2D, tex.get_texnum());
	check_error();

	GLenum type, internal_format;
	void *pixels;
	assert((weights.bilinear_weights_fp16 == nullptr) != (weights.bilinear_weights_fp32 == nullptr));
	if (weights.bilinear_weights_fp32 != nullptr) {
		type = GL_FLOAT;
		internal_format = GL_RG32F;
		pixels = weights.bilinear_weights_fp32.get();
	} else {
		type = GL_HALF_FLOAT;
		internal_format = GL_RG16F;
		pixels = weights.bilinear_weights_fp16.get();
	}

	tex.update(weights.src_bilinear_samples, weights.dst_samples, internal_format, GL_RG, type, pixels);
}

namespace {

ScalingWeights calculate_scaling_weights(unsigned src_size, unsigned dst_size, float zoom, float offset)
{
	// Only needed if run from outside ResampleEffect.
	call_once(lanczos_table_init_done, init_lanczos_table);

	// For many resamplings (e.g. 640 -> 1280), we will end up with the same
	// set of samples over and over again in a loop. Thus, we can compute only
	// the first such loop, and then ask the card to repeat the texture for us.
	// This is both easier on the texture cache and lowers our CPU cost for
	// generating the kernel somewhat.
	float scaling_factor;
	int num_loops;
	if (fabs(zoom - 1.0f) < 1e-6) {
		num_loops = gcd(src_size, dst_size);
		scaling_factor = float(dst_size) / float(src_size);
	} else {
		// If zooming is enabled (ie., zoom != 1), we turn off the looping.
		// We _could_ perhaps do it for rational zoom levels (especially
		// things like 2:1), but it doesn't seem to be worth it, given that
		// the most common use case would seem to be varying the zoom
		// from frame to frame.
		num_loops = 1;
		scaling_factor = zoom * float(dst_size) / float(src_size);
	}
	unsigned dst_samples = dst_size / num_loops;

	// Sample the kernel in the right place. A diagram with a triangular kernel
	// (corresponding to linear filtering, and obviously with radius 1)
	// for easier ASCII art drawing:
	//
	//                *
	//               / \                      |
	//              /   \                     |
	//             /     \                    |
	//    x---x---x   x   x---x---x---x
	//
	// Scaling up (in this case, 2x) means sampling more densely:
	//
	//                *
	//               / \                      |
	//              /   \                     |
	//             /     \                    |
	//   x-x-x-x-x-x x x x-x-x-x-x-x-x-x
	//
	// When scaling up, any destination pixel will only be influenced by a few
	// (in this case, two) neighboring pixels, and more importantly, the number
	// will not be influenced by the scaling factor. (Note, however, that the
	// pixel centers have moved, due to OpenGL's center-pixel convention.)
	// The only thing that changes is the weights themselves, as the sampling
	// points are at different distances from the original pixels.
	//
	// Scaling down is a different story:
	//
	//                *
	//               / \                      |
	//              /   \                     |
	//             /     \                    |
	//    --x------ x     --x-------x--
	//
	// Again, the pixel centers have moved in a maybe unintuitive fashion,
	// although when you consider that there are multiple source pixels around,
	// it's not so bad as at first look:
	//
	//            *   *   *   *
	//           / \ / \ / \ / \              |
	//          /   X   X   X   \             |
	//         /   / \ / \ / \   \            |
	//    --x-------x-------x-------x--
	//
	// As you can see, the new pixels become averages of the two neighboring old
	// ones (the situation for Lanczos is of course more complex).
	//
	// Anyhow, in this case we clearly need to look at more source pixels
	// to compute the destination pixel, and how many depend on the scaling factor.
	// Thus, the kernel width will vary with how much we scale.
	float radius_scaling_factor = min(scaling_factor, 1.0f);
	const int int_radius = lrintf(LANCZOS_RADIUS / radius_scaling_factor);
	const int src_samples = int_radius * 2 + 1;
	unique_ptr<Tap<float>[]> weights(new Tap<float>[dst_samples * src_samples]);
	float subpixel_offset = offset - lrintf(offset);  // The part not covered by whole_pixel_offset.
	assert(subpixel_offset >= -0.5f && subpixel_offset <= 0.5f);
	float inv_scaling_factor = 1.0f / scaling_factor;
	for (unsigned y = 0; y < dst_samples; ++y) {
		// Find the point around which we want to sample the source image,
		// compensating for differing pixel centers as the scale changes.
		float center_src_y = (y + 0.5f) * inv_scaling_factor - 0.5f;
		int base_src_y = lrintf(center_src_y);

		// Now sample <int_radius> pixels on each side around that point.
		float inv_src_size = 1.0 / float(src_size);
		for (int i = 0; i < src_samples; ++i) {
			int src_y = base_src_y + i - int_radius;
			float weight = lanczos_weight_cached(radius_scaling_factor * (src_y - center_src_y - subpixel_offset));
			weights[y * src_samples + i].weight = weight * radius_scaling_factor;
			weights[y * src_samples + i].pos = (src_y + 0.5f) * inv_src_size;
		}
	}

	ScalingWeights ret;
	ret.src_bilinear_samples = src_samples;
	ret.dst_samples = dst_samples;
	ret.num_loops = num_loops;
	ret.bilinear_weights_fp16 = nullptr;
	ret.bilinear_weights_fp32 = move(weights);
	return ret;
}

}  // namespace

ScalingWeights calculate_bilinear_scaling_weights(unsigned src_size, unsigned dst_size, float zoom, float offset)
{
	ScalingWeights ret = calculate_scaling_weights(src_size, dst_size, zoom, offset);
	unique_ptr<Tap<float>[]> weights = move(ret.bilinear_weights_fp32);
	const int src_samples = ret.src_bilinear_samples;

	// Now make use of the bilinear filtering in the GPU to reduce the number of samples
	// we need to make. Try fp16 first; if it's not accurate enough, we go to fp32.
	// Our tolerance level for total error is a bit higher than the one for invididual
	// samples, since one would assume overall errors in the shape don't matter as much.
	const float max_error = 2.0f / (255.0f * 255.0f);
	unique_ptr<Tap<fp16_int_t>[]> bilinear_weights_fp16;
	int src_bilinear_samples = combine_many_samples(weights.get(), src_size, src_samples, ret.dst_samples, &bilinear_weights_fp16);
	unique_ptr<Tap<float>[]> bilinear_weights_fp32 = nullptr;
	double max_sum_sq_error_fp16 = 0.0;
	for (unsigned y = 0; y < ret.dst_samples; ++y) {
		double sum_sq_error_fp16 = compute_sum_sq_error(
			weights.get() + y * src_samples, src_samples,
			bilinear_weights_fp16.get() + y * src_bilinear_samples, src_bilinear_samples,
			src_size);
		max_sum_sq_error_fp16 = std::max(max_sum_sq_error_fp16, sum_sq_error_fp16);
		if (max_sum_sq_error_fp16 > max_error) {
			break;
		}
	}

	if (max_sum_sq_error_fp16 > max_error) {
		bilinear_weights_fp16.reset();
		src_bilinear_samples = combine_many_samples(weights.get(), src_size, src_samples, ret.dst_samples, &bilinear_weights_fp32);
	}

	ret.src_bilinear_samples = src_bilinear_samples;
	ret.bilinear_weights_fp16 = move(bilinear_weights_fp16);
	ret.bilinear_weights_fp32 = move(bilinear_weights_fp32);
	return ret;
}

void SingleResamplePassEffect::set_gl_state(GLuint glsl_program_num, const string &prefix, unsigned *sampler_num)
{
	Effect::set_gl_state(glsl_program_num, prefix, sampler_num);

	assert(input_width > 0);
	assert(input_height > 0);
	assert(output_width > 0);
	assert(output_height > 0);

	if (input_width != last_input_width ||
	    input_height != last_input_height ||
	    output_width != last_output_width ||
	    output_height != last_output_height ||
	    offset != last_offset ||
	    zoom != last_zoom) {
		update_texture(glsl_program_num, prefix, sampler_num);
		last_input_width = input_width;
		last_input_height = input_height;
		last_output_width = output_width;
		last_output_height = output_height;
		last_offset = offset;
		last_zoom = zoom;
	}

	glActiveTexture(GL_TEXTURE0 + *sampler_num);
	check_error();
	glBindTexture(GL_TEXTURE_2D, tex.get_texnum());
	check_error();

	uniform_sample_tex = *sampler_num;
	++*sampler_num;
	uniform_num_samples = src_bilinear_samples;
	uniform_num_loops = num_loops;
	uniform_slice_height = slice_height;

	// Instructions for how to convert integer sample numbers to positions in the weight texture.
	uniform_sample_x_scale = 1.0f / src_bilinear_samples;
	uniform_sample_x_offset = 0.5f / src_bilinear_samples;

	if (direction == SingleResamplePassEffect::VERTICAL) {
		uniform_whole_pixel_offset = lrintf(offset) / float(input_height);
	} else {
		uniform_whole_pixel_offset = lrintf(offset) / float(input_width);
	}
}

Support2DTexture::Support2DTexture()
{
	glGenTextures(1, &texnum);
	check_error();
	glBindTexture(GL_TEXTURE_2D, texnum);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	check_error();
}

Support2DTexture::~Support2DTexture()
{
	glDeleteTextures(1, &texnum);
	check_error();
}

void Support2DTexture::update(GLint width, GLint height, GLenum internal_format, GLenum format, GLenum type, const GLvoid * data)
{
	glBindTexture(GL_TEXTURE_2D, texnum);
	check_error();
	if (width == last_texture_width &&
	    height == last_texture_height &&
	    internal_format == last_texture_internal_format) {
		// Texture dimensions and type are unchanged; it is more efficient
		// to just update it rather than making an entirely new texture.
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, type, data);
		check_error();
	} else {
		glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, type, data);
		check_error();
		last_texture_width = width;
		last_texture_height = height;
		last_texture_internal_format = internal_format;
	}
}

}  // namespace movit
