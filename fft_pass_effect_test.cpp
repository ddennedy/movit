// Unit tests for FFTPassEffect.

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <epoxy/gl.h>
#include <gtest/gtest.h>

#include "effect_chain.h"
#include "fft_pass_effect.h"
#include "image_format.h"
#include "multiply_effect.h"
#include "test_util.h"

namespace movit {

namespace {

// Generate a random number uniformly distributed between [-1.0, 1.0].
float uniform_random()
{
	return 2.0 * ((float)rand() / RAND_MAX - 0.5);
}

void setup_fft(EffectChain *chain, int fft_size, bool inverse,
               bool add_normalizer = false,
               FFTPassEffect::Direction direction = FFTPassEffect::HORIZONTAL)
{
	assert((fft_size & (fft_size - 1)) == 0);  // Must be power of two.
	for (int i = 1, subsize = 2; subsize <= fft_size; ++i, subsize *= 2) {
		Effect *fft_effect = chain->add_effect(new FFTPassEffect());
		bool ok = fft_effect->set_int("fft_size", fft_size);
		ok |= fft_effect->set_int("pass_number", i);
		ok |= fft_effect->set_int("inverse", inverse);
		ok |= fft_effect->set_int("direction", direction);
		assert(ok);
	}

	if (add_normalizer) {
		float factor[4] = { 1.0f / fft_size, 1.0f / fft_size, 1.0f / fft_size, 1.0f / fft_size };
		Effect *multiply_effect = chain->add_effect(new MultiplyEffect());
		bool ok = multiply_effect->set_vec4("factor", factor);
		assert(ok);
	}
}

void run_fft(const float *in, float *out, int fft_size, bool inverse,
             bool add_normalizer = false,
             FFTPassEffect::Direction direction = FFTPassEffect::HORIZONTAL)
{
	int width, height;
	if (direction == FFTPassEffect::HORIZONTAL) {
		width = fft_size;
		height = 1;
	} else {
		width = 1;
		height = fft_size;
	}
	EffectChainTester tester(in, width, height, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	setup_fft(tester.get_chain(), fft_size, inverse, add_normalizer, direction);
	tester.run(out, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);
}

}  // namespace

TEST(FFTPassEffectTest, ZeroStaysZero) {
	const int fft_size = 64;
	float data[fft_size * 4] = { 0 };
	float out_data[fft_size * 4];

	run_fft(data, out_data, fft_size, false);
	expect_equal(data, out_data, 4, fft_size);

	run_fft(data, out_data, fft_size, true);
	expect_equal(data, out_data, 4, fft_size);
}

TEST(FFTPassEffectTest, Impulse) {
	const int fft_size = 64;
	float data[fft_size * 4] = { 0 };
	float expected_data[fft_size * 4], out_data[fft_size * 4];
	data[0] = 1.0;
	data[1] = 1.2;
	data[2] = 1.4;
	data[3] = 3.0;

	for (int i = 0; i < fft_size; ++i) {
		expected_data[i * 4 + 0] = data[0];
		expected_data[i * 4 + 1] = data[1];
		expected_data[i * 4 + 2] = data[2];
		expected_data[i * 4 + 3] = data[3];
	}

	run_fft(data, out_data, fft_size, false);
	expect_equal(expected_data, out_data, 4, fft_size);

	run_fft(data, out_data, fft_size, true);
	expect_equal(expected_data, out_data, 4, fft_size);
}

TEST(FFTPassEffectTest, SingleFrequency) {
	const int fft_size = 16;
	float data[fft_size * 4] = { 0 };
	float expected_data[fft_size * 4], out_data[fft_size * 4];
	for (int i = 0; i < fft_size; ++i) {
		data[i * 4 + 0] = sin(2.0 * M_PI * (4.0 * i) / fft_size);
		data[i * 4 + 1] = 0.0;
		data[i * 4 + 2] = 0.0;
		data[i * 4 + 3] = 0.0;
	}
	for (int i = 0; i < fft_size; ++i) {
		expected_data[i * 4 + 0] = 0.0;
		expected_data[i * 4 + 1] = 0.0;
		expected_data[i * 4 + 2] = 0.0;
		expected_data[i * 4 + 3] = 0.0;
	}
	expected_data[4 * 4 + 1] = -8.0;
	expected_data[12 * 4 + 1] = 8.0;

	run_fft(data, out_data, fft_size, false, false, FFTPassEffect::HORIZONTAL);
	expect_equal(expected_data, out_data, 4, fft_size);

	run_fft(data, out_data, fft_size, false, false, FFTPassEffect::VERTICAL);
	expect_equal(expected_data, out_data, 4, fft_size);
}

TEST(FFTPassEffectTest, Repeat) {
	srand(12345);
	for (int fft_size = 2; fft_size <= 128; fft_size *= 2) {
		const int num_repeats = 31;  // Prime, to make things more challenging.
		float data[num_repeats * fft_size * 4];
		float expected_data[num_repeats * fft_size * 4], out_data[num_repeats * fft_size * 4];

		for (int i = 0; i < num_repeats * fft_size * 4; ++i) {
			data[i] = uniform_random();
		}

		for (int i = 0; i < num_repeats; ++i) {
			run_fft(data + i * fft_size * 4, expected_data + i * fft_size * 4, fft_size, false);
		}

		{
			// Horizontal.
			EffectChainTester tester(data, num_repeats * fft_size, 1, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
			setup_fft(tester.get_chain(), fft_size, false);
			tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

			expect_equal(expected_data, out_data, 4, num_repeats * fft_size);
		}
		{
			// Vertical.
			EffectChainTester tester(data, 1, num_repeats * fft_size, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
			setup_fft(tester.get_chain(), fft_size, false, false, FFTPassEffect::VERTICAL);
			tester.run(out_data, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

			expect_equal(expected_data, out_data, 4, num_repeats * fft_size);
		}
	}
}

TEST(FFTPassEffectTest, TwoDimensional) {  // Implicitly tests vertical.
	srand(1234);
	const int fft_size = 16;
	float in[fft_size * fft_size * 4], out[fft_size * fft_size * 4], expected_out[fft_size * fft_size * 4];
	for (int y = 0; y < fft_size; ++y) {
		for (int x = 0; x < fft_size; ++x) {
			in[(y * fft_size + x) * 4 + 0] =
				sin(2.0 * M_PI * (2 * x + 3 * y) / fft_size);
			in[(y * fft_size + x) * 4 + 1] = 0.0;
			in[(y * fft_size + x) * 4 + 2] = 0.0;
			in[(y * fft_size + x) * 4 + 3] = 0.0;
		}
	}
	memset(expected_out, 0, sizeof(expected_out));

	// This result has been verified using the fft2() function in Octave,
	// which uses FFTW.
	expected_out[(3 * fft_size + 2) * 4 + 1] = -128.0;
	expected_out[(13 * fft_size + 14) * 4 + 1] = 128.0;

	EffectChainTester tester(in, fft_size, fft_size, FORMAT_RGBA_PREMULTIPLIED_ALPHA, COLORSPACE_sRGB, GAMMA_LINEAR);
	setup_fft(tester.get_chain(), fft_size, false, false, FFTPassEffect::HORIZONTAL);
	setup_fft(tester.get_chain(), fft_size, false, false, FFTPassEffect::VERTICAL);
	tester.run(out, GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);

	expect_equal(expected_out, out, 4 * fft_size, fft_size, 0.25, 0.0005);
}

// The classic paper for FFT correctness testing is Funda Ergün:
// “Testing Multivariate Linear Functions: Overcoming the Generator Bottleneck”
// (http://www.cs.sfu.ca/~funda/PUBLICATIONS/stoc95.ps), which proves that
// testing three basic properties of FFTs guarantees that the function is
// correct (at least under the assumption that errors are random).
//
// We don't follow the paper directly, though, for a few reasons: First,
// Ergün's paper really considers _self-correcting_ systems, which may
// be stochastically faulty, and thus uses various relatively complicated
// bounds and tests we don't really need. Second, the FFTs it considers
// are all about polynomials over finite fields, which means that results
// are exact and thus easy to test; we work with floats (half-floats!),
// and thus need some error tolerance.
//
// So instead, we follow the implementation of FFTW, which is really the
// gold standard when it comes to FFTs these days. They hard-code 20
// testing rounds as opposed to the more complicated bounds in the paper,
// and have a simpler version of the third test.
//
// The error bounds are set somewhat empirically, but remember that these
// inputs will give frequency values as large as ~16, where 0.025 is
// within the 9th bit (of 11 total mantissa bits in fp16).
const int ergun_rounds = 20;

// Test 1: Test that FFT(a + b) = FFT(a) + FFT(b).
TEST(FFTPassEffectTest, ErgunLinearityTest) {
	srand(1234);
	const int max_fft_size = 64;
	float a[max_fft_size * 4], b[max_fft_size * 4], sum[max_fft_size * 4];
	float a_out[max_fft_size * 4], b_out[max_fft_size * 4], sum_out[max_fft_size * 4], expected_sum_out[max_fft_size * 4];
	for (int fft_size = 2; fft_size <= max_fft_size; fft_size *= 2) {
		for (int inverse = 0; inverse <= 1; ++inverse) {
			for (int i = 0; i < ergun_rounds; ++i) {
				for (int j = 0; j < fft_size * 4; ++j) {
					a[j] = uniform_random();
					b[j] = uniform_random();
				}
				run_fft(a, a_out, fft_size, inverse);
				run_fft(b, b_out, fft_size, inverse);

				for (int j = 0; j < fft_size * 4; ++j) {
					sum[j] = a[j] + b[j];
					expected_sum_out[j] = a_out[j] + b_out[j];
				}

				run_fft(sum, sum_out, fft_size, inverse);
				expect_equal(expected_sum_out, sum_out, 4, fft_size, 0.03, 0.0005);
			}
		}
	}
}

// Test 2: Test that FFT(delta(i)) = 1  (where delta(i) = [1 0 0 0 ...]),
// or more specifically, test that FFT(a + delta(i)) - FFT(a) = 1.
TEST(FFTPassEffectTest, ErgunImpulseTransform) {
	srand(1235);
	const int max_fft_size = 64;
	float a[max_fft_size * 4], b[max_fft_size * 4];
	float a_out[max_fft_size * 4], b_out[max_fft_size * 4], sum_out[max_fft_size * 4], expected_sum_out[max_fft_size * 4];
	for (int fft_size = 2; fft_size <= max_fft_size; fft_size *= 2) {
		for (int inverse = 0; inverse <= 1; ++inverse) {
			for (int i = 0; i < ergun_rounds; ++i) {
				for (int j = 0; j < fft_size * 4; ++j) {
					a[j] = uniform_random();

					// Compute delta(j) - a.
					if (j < 4) {
						b[j] = 1.0 - a[j];
					} else {
						b[j] = -a[j];
					}
				}
				run_fft(a, a_out, fft_size, inverse);
				run_fft(b, b_out, fft_size, inverse);

				for (int j = 0; j < fft_size * 4; ++j) {
					sum_out[j] = a_out[j] + b_out[j];
					expected_sum_out[j] = 1.0;
				}
				expect_equal(expected_sum_out, sum_out, 4, fft_size, 0.025, 0.0005);
			}
		}
	}
}

// Test 3: Test the time-shift property of the FFT, in that a circular left-shift
// multiplies the result by e^(j 2pi k/N) (linear phase adjustment).
// As fftw_test.c says, “The paper performs more tests, but this code should be
// fine too”.
TEST(FFTPassEffectTest, ErgunShiftProperty) {
	srand(1236);
	const int max_fft_size = 64;
	float a[max_fft_size * 4], b[max_fft_size * 4];
	float a_out[max_fft_size * 4], b_out[max_fft_size * 4], expected_a_out[max_fft_size * 4];
	for (int fft_size = 2; fft_size <= max_fft_size; fft_size *= 2) {
		for (int inverse = 0; inverse <= 1; ++inverse) {
			for (int direction = 0; direction <= 1; ++direction) {
				for (int i = 0; i < ergun_rounds; ++i) {
					for (int j = 0; j < fft_size * 4; ++j) {
						a[j] = uniform_random();
					}

					// Circular shift left by one step.
					for (int j = 0; j < fft_size * 4; ++j) {
						b[j] = a[(j + 4) % (fft_size * 4)];
					}
					run_fft(a, a_out, fft_size, inverse, false, FFTPassEffect::Direction(direction));
					run_fft(b, b_out, fft_size, inverse, false, FFTPassEffect::Direction(direction));

					for (int j = 0; j < fft_size; ++j) {
						double s = -sin(j * 2.0 * M_PI / fft_size);
						double c = cos(j * 2.0 * M_PI / fft_size);
						if (inverse) {
							s = -s;
						}

						expected_a_out[j * 4 + 0] = b_out[j * 4 + 0] * c - b_out[j * 4 + 1] * s;
						expected_a_out[j * 4 + 1] = b_out[j * 4 + 0] * s + b_out[j * 4 + 1] * c;

						expected_a_out[j * 4 + 2] = b_out[j * 4 + 2] * c - b_out[j * 4 + 3] * s;
						expected_a_out[j * 4 + 3] = b_out[j * 4 + 2] * s + b_out[j * 4 + 3] * c;
					}
					expect_equal(expected_a_out, a_out, 4, fft_size, 0.025, 0.0005);
				}
			}
		}
	}
}

TEST(FFTPassEffectTest, BigFFTAccuracy) {
	srand(1234);
	const int max_fft_size = 2048;
	float in[max_fft_size * 4], out[max_fft_size * 4], out2[max_fft_size * 4];
	for (int fft_size = 2; fft_size <= max_fft_size; fft_size *= 2) {
		for (int j = 0; j < fft_size * 4; ++j) {
			in[j] = uniform_random();
		}
		run_fft(in, out, fft_size, false, true);  // Forward, with normalization.
		run_fft(out, out2, fft_size, true);       // Reverse.

		// These error bounds come from
		// http://en.wikipedia.org/wiki/Fast_Fourier_transform#Accuracy_and_approximations,
		// with empirically estimated epsilons. Note that the calculated
		// rms in expect_equal() is divided by sqrt(N), so we compensate
		// similarly here.
		double max_error = 0.0009 * log2(fft_size);
		double rms_limit = 0.0007 * sqrt(log2(fft_size)) / sqrt(fft_size);
		expect_equal(in, out2, 4, fft_size, max_error, rms_limit);
	}
}

}  // namespace movit
