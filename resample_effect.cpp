// Three-lobed Lanczos, the most common choice.
#define LANCZOS_RADIUS 3.0

#include <epoxy/gl.h>
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <algorithm>
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

template<class T>
struct Tap {
	T weight;
	T pos;
};

float sinc(float x)
{
	if (fabs(x) < 1e-6) {
		return 1.0f - fabs(x);
	} else {
		return sin(x) / x;
	}
}

float lanczos_weight(float x, float a)
{
	if (fabs(x) > a) {
		return 0.0f;
	} else {
		return sinc(M_PI * x) * sinc(M_PI * x / a);
	}
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
unsigned combine_samples(const Tap<float> *src, Tap<DestFloat> *dst, unsigned src_size, unsigned num_src_samples, unsigned max_samples_saved)
{
	unsigned num_samples_saved = 0;
	for (unsigned i = 0, j = 0; i < num_src_samples; ++i, ++j) {
		// Copy the sample directly; it will be overwritten later if we can combine.
		if (dst != NULL) {
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

		fp16_int_t pos, total_weight;
		float sum_sq_error;
		combine_two_samples(w1, w2, pos1, pos2, src_size, &pos, &total_weight, &sum_sq_error);

		// If the interpolation error is larger than that of about sqrt(2) of
		// a level at 8-bit precision, don't combine. (You'd think 1.0 was enough,
		// but since the artifacts are not really random, they can get quite
		// visible. On the other hand, going to 0.25f, I can see no change at
		// all with 8-bit output, so it would not seem to be worth it.)
		if (sum_sq_error > 0.5f / (255.0f * 255.0f)) {
			continue;
		}

		// OK, we can combine this and the next sample.
		if (dst != NULL) {
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
		double sum = 0.0;
		for (unsigned i = 0; i < num; ++i) {
			sum += to_fp64(vals[i].weight);
		}
		for (unsigned i = 0; i < num; ++i) {
			vals[i].weight = from_fp64<T>(to_fp64(vals[i].weight) / sum);
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
unsigned combine_many_samples(const Tap<float> *weights, unsigned src_size, unsigned src_samples, unsigned dst_samples, Tap<DestFloat> **bilinear_weights)
{
	int src_bilinear_samples = 0;
	for (unsigned y = 0; y < dst_samples; ++y) {
		unsigned num_samples_saved = combine_samples<DestFloat>(weights + y * src_samples, NULL, src_size, src_samples, UINT_MAX);
		src_bilinear_samples = max<int>(src_bilinear_samples, src_samples - num_samples_saved);
	}

	// Now that we know the right width, actually combine the samples.
	*bilinear_weights = new Tap<DestFloat>[dst_samples * src_bilinear_samples];
	for (unsigned y = 0; y < dst_samples; ++y) {
		Tap<DestFloat> *bilinear_weights_ptr = *bilinear_weights + y * src_bilinear_samples;
		unsigned num_samples_saved = combine_samples(
			weights + y * src_samples,
			bilinear_weights_ptr,
			src_size,
			src_samples,
			src_samples - src_bilinear_samples);
		assert(int(src_samples) - int(num_samples_saved) == src_bilinear_samples);
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
	int lower_pos = int(floor(to_fp64(bilinear_weights[0].pos) * size - 0.5));
	int upper_pos = int(ceil(to_fp64(bilinear_weights[num_bilinear_weights - 1].pos) * size - 0.5)) + 2;
	lower_pos = min<int>(lower_pos, lrintf(weights[0].pos * size - 0.5));
	upper_pos = max<int>(upper_pos, lrintf(weights[num_weights - 1].pos * size - 0.5));

	float* effective_weights = new float[upper_pos - lower_pos];
	for (int i = 0; i < upper_pos - lower_pos; ++i) {
		effective_weights[i] = 0.0f;
	}

	// Now find the effective weights that result from this sampling.
	for (unsigned i = 0; i < num_bilinear_weights; ++i) {
		const float pixel_pos = to_fp64(bilinear_weights[i].pos) * size - 0.5f;
		const int x0 = int(floor(pixel_pos)) - lower_pos;
		const int x1 = x0 + 1;
		const float f = lrintf((pixel_pos - (x0 + lower_pos)) / movit_texel_subpixel_precision) * movit_texel_subpixel_precision;

		assert(x0 >= 0);
		assert(x1 >= 0);
		assert(x0 < upper_pos - lower_pos);
		assert(x1 < upper_pos - lower_pos);

		effective_weights[x0] += to_fp64(bilinear_weights[i].weight) * (1.0 - f);
		effective_weights[x1] += to_fp64(bilinear_weights[i].weight) * f;
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

// Given a predefined, fixed set of bilinear weight positions, try to optimize
// their weights through some linear algebra. This can do a better job than
// the weight calculation in combine_samples() because it can look at the entire
// picture (an effective weight can sometimes be affected by multiple samples).
// It will also optimize weights for non-combined samples, which is useful when
// a sample happens in-between texels for numerical reasons.
//
// The math goes as follows: The desired result is a weighted sum, where the
// weights are the coefficients in <weights>:
//
//   y = sum(c_j x_j, j)
//
// We try to approximate this by a different set of coefficients, which have
// weights d_i and are placed at some fraction to the right of a source texel x_j.
// This means it will influence two texels (x_j and x_{j+1}); generalizing this,
// let us define that w_ij means the amount texel <j> influences bilinear weight
// <i> (keeping in mind that w_ij = 0 for all but at most two different j).
// This means the actually computed result is:
//
//   y' = sum(d_i w_ij x_j, j)
//
// We assume w_ij fixed and wish to find {d_i} so that y' gets as close to y
// as possible. Specifically, let us consider the sum of squred errors of the
// coefficients:
//
//   ε² = sum((sum( d_i w_ij, i ) - c_j)², j)
//
// The standard trick, which also applies just fine here, is to differentiate
// the error with respect to each variable we wish to optimize, and set each
// such expression to zero. Solving this equation set (which we can do efficiently
// by letting Eigen invert a sparse matrix for us) yields the minimum possible
// error. To see the form each such equation takes, pick any value k and
// differentiate the expression by d_k:
//
//   ∂(ε²)/∂(d_k) = sum(2(sum( d_i w_ij, i ) - c_j) w_kj, j)
//
// Setting this expression equal to zero, dropping the irrelevant factor 2 and
// rearranging yields:
//
//   sum(w_kj sum( d_i w_ij, i ), j) = sum(w_kj c_j, j)
//
// where again, we remember where the sums over j are over at most two elements,
// since w_ij is nonzero for at most two values of j.
template<class T>
void optimize_sum_sq_error(const Tap<float>* weights, unsigned num_weights,
                           Tap<T>* bilinear_weights, unsigned num_bilinear_weights,
                           unsigned size)
{
	// Find the range of the desired weights.
	int c_lower_pos = lrintf(weights[0].pos * size - 0.5);
	int c_upper_pos = lrintf(weights[num_weights - 1].pos * size - 0.5) + 1;

	SparseMatrix<float> A(num_bilinear_weights, num_bilinear_weights);
	SparseVector<float> b(num_bilinear_weights);

	// Convert each bilinear weight to the (x, frac) form for less junk in the code below.
	int* pos = new int[num_bilinear_weights];
	float* fracs = new float[num_bilinear_weights];
	for (unsigned i = 0; i < num_bilinear_weights; ++i) {
		const float pixel_pos = to_fp64(bilinear_weights[i].pos) * size - 0.5f;
		const float f = pixel_pos - floor(pixel_pos);
		pos[i] = int(floor(pixel_pos));
		fracs[i] = lrintf(f / movit_texel_subpixel_precision) * movit_texel_subpixel_precision;
	}

	// The index ordering is a bit unusual to fit better with the
	// notation in the derivation above.
	for (unsigned k = 0; k < num_bilinear_weights; ++k) {
		for (int j = pos[k]; j <= pos[k] + 1; ++j) {
			const float f_kj = (j == pos[k]) ? (1.0f - fracs[k]) : fracs[k];
			for (unsigned i = 0; i < num_bilinear_weights; ++i) {
				float f_ij;
				if (j == pos[i]) {
					f_ij = 1.0f - fracs[i];
				} else if (j == pos[i] + 1) {
					f_ij = fracs[i];
				} else {
					// f_ij = 0
					continue;
				}
				A.coeffRef(i, k) += f_kj * f_ij;
			}
			float c_j;
			if (j >= c_lower_pos && j < c_upper_pos) {
				c_j = weights[j - c_lower_pos].weight;
			} else {
				c_j = 0.0f;
			}
			b.coeffRef(k) += f_kj * c_j;
		}
	}
	delete[] pos;
	delete[] fracs;

	A.makeCompressed();
	SparseQR<SparseMatrix<float>, COLAMDOrdering<int> > qr(A);
	assert(qr.info() == Success);
	SparseMatrix<float> new_weights = qr.solve(b);
	assert(qr.info() == Success);

	for (unsigned i = 0; i < num_bilinear_weights; ++i) {
		bilinear_weights[i].weight = from_fp64<T>(new_weights.coeff(i, 0));
	}
	normalize_sum(bilinear_weights, num_bilinear_weights);
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
	hpass = new SingleResamplePassEffect(this);
	CHECK(hpass->set_int("direction", SingleResamplePassEffect::HORIZONTAL));
	vpass = new SingleResamplePassEffect(NULL);
	CHECK(vpass->set_int("direction", SingleResamplePassEffect::VERTICAL));

	update_size();
}

void ResampleEffect::rewrite_graph(EffectChain *graph, Node *self)
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

	glGenTextures(1, &texnum);
}

SingleResamplePassEffect::~SingleResamplePassEffect()
{
	glDeleteTextures(1, &texnum);
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

	// For many resamplings (e.g. 640 -> 1280), we will end up with the same
	// set of samples over and over again in a loop. Thus, we can compute only
	// the first such loop, and then ask the card to repeat the texture for us.
	// This is both easier on the texture cache and lowers our CPU cost for
	// generating the kernel somewhat.
	float scaling_factor;
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
	slice_height = 1.0f / num_loops;
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
	int int_radius = lrintf(LANCZOS_RADIUS / radius_scaling_factor);
	int src_samples = int_radius * 2 + 1;
	Tap<float> *weights = new Tap<float>[dst_samples * src_samples];
	float subpixel_offset = offset - lrintf(offset);  // The part not covered by whole_pixel_offset.
	assert(subpixel_offset >= -0.5f && subpixel_offset <= 0.5f);
	for (unsigned y = 0; y < dst_samples; ++y) {
		// Find the point around which we want to sample the source image,
		// compensating for differing pixel centers as the scale changes.
		float center_src_y = (y + 0.5f) / scaling_factor - 0.5f;
		int base_src_y = lrintf(center_src_y);

		// Now sample <int_radius> pixels on each side around that point.
		for (int i = 0; i < src_samples; ++i) {
			int src_y = base_src_y + i - int_radius;
			float weight = lanczos_weight(radius_scaling_factor * (src_y - center_src_y - subpixel_offset), LANCZOS_RADIUS);
			weights[y * src_samples + i].weight = weight * radius_scaling_factor;
			weights[y * src_samples + i].pos = (src_y + 0.5) / float(src_size);
		}
	}

	// Now make use of the bilinear filtering in the GPU to reduce the number of samples
	// we need to make. Try fp16 first; if it's not accurate enough, we go to fp32.
	Tap<fp16_int_t> *bilinear_weights_fp16;
	src_bilinear_samples = combine_many_samples(weights, src_size, src_samples, dst_samples, &bilinear_weights_fp16);
	Tap<float> *bilinear_weights_fp32 = NULL;
	bool fallback_to_fp32 = false;
	double max_sum_sq_error_fp16 = 0.0;
	for (unsigned y = 0; y < dst_samples; ++y) {
		optimize_sum_sq_error(
			weights + y * src_samples, src_samples,
			bilinear_weights_fp16 + y * src_bilinear_samples, src_bilinear_samples,
			src_size);
		double sum_sq_error_fp16 = compute_sum_sq_error(
			weights + y * src_samples, src_samples,
			bilinear_weights_fp16 + y * src_bilinear_samples, src_bilinear_samples,
			src_size);
		max_sum_sq_error_fp16 = std::max(max_sum_sq_error_fp16, sum_sq_error_fp16);
	}

	// Our tolerance level for total error is a bit higher than the one for invididual
	// samples, since one would assume overall errors in the shape don't matter as much.
	if (max_sum_sq_error_fp16 > 2.0f / (255.0f * 255.0f)) {
		fallback_to_fp32 = true;
		src_bilinear_samples = combine_many_samples(weights, src_size, src_samples, dst_samples, &bilinear_weights_fp32);
		for (unsigned y = 0; y < dst_samples; ++y) {
			optimize_sum_sq_error(
				weights + y * src_samples, src_samples,
				bilinear_weights_fp32 + y * src_bilinear_samples, src_bilinear_samples,
				src_size);
		}
	}

	// Encode as a two-component texture. Note the GL_REPEAT.
	glActiveTexture(GL_TEXTURE0 + *sampler_num);
	check_error();
	glBindTexture(GL_TEXTURE_2D, texnum);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	check_error();
	if (fallback_to_fp32) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, src_bilinear_samples, dst_samples, 0, GL_RG, GL_FLOAT, bilinear_weights_fp32);
	} else {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, src_bilinear_samples, dst_samples, 0, GL_RG, GL_HALF_FLOAT, bilinear_weights_fp16);
	}
	check_error();

	delete[] weights;
	delete[] bilinear_weights_fp16;
	delete[] bilinear_weights_fp32;
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
	glBindTexture(GL_TEXTURE_2D, texnum);
	check_error();

	set_uniform_int(glsl_program_num, prefix, "sample_tex", *sampler_num);
	++*sampler_num;
	set_uniform_int(glsl_program_num, prefix, "num_samples", src_bilinear_samples);
	set_uniform_float(glsl_program_num, prefix, "num_loops", num_loops);
	set_uniform_float(glsl_program_num, prefix, "slice_height", slice_height);

	// Instructions for how to convert integer sample numbers to positions in the weight texture.
	set_uniform_float(glsl_program_num, prefix, "sample_x_scale", 1.0f / src_bilinear_samples);
	set_uniform_float(glsl_program_num, prefix, "sample_x_offset", 0.5f / src_bilinear_samples);

	float whole_pixel_offset;
	if (direction == SingleResamplePassEffect::VERTICAL) {
		whole_pixel_offset = lrintf(offset) / float(input_height);
	} else {
		whole_pixel_offset = lrintf(offset) / float(input_width);
	}
	set_uniform_float(glsl_program_num, prefix, "whole_pixel_offset", whole_pixel_offset);

	// We specifically do not want mipmaps on the input texture;
	// they break minification.
	Node *self = chain->find_node_for_effect(this);
	glActiveTexture(chain->get_input_sampler(self, 0));
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	check_error();
}

}  // namespace movit
