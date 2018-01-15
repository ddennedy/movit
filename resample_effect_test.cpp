// Unit tests for ResampleEffect.

#include <epoxy/gl.h>
#include <gtest/gtest.h>
#include <math.h>

#include <memory>

#include "effect_chain.h"
#include "flat_input.h"
#include "fp16.h"
#include "image_format.h"
#include "init.h"
#include "resample_effect.h"
#include "test_util.h"

using namespace std;

namespace movit {

namespace {

float sinc(float x)
{
	return sin(M_PI * x) / (M_PI * x);
}

float lanczos(float x, float a)
{
	if (fabs(x) >= a) {
		return 0.0f;
	} else {
		return sinc(x) * sinc(x / a);
	}
}

}  // namespace

TEST(ResampleEffectTest, IdentityTransformDoesNothing) {
	const int size = 4;

	float data[size * size] = {
		0.0, 1.0, 0.0, 1.0,
		0.0, 1.0, 1.0, 0.0,
		0.0, 0.5, 1.0, 0.5,
		0.0, 0.0, 0.0, 0.0,
	};
	float out_data[size * size];

	EffectChainTester tester(data, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *resample_effect = tester.get_chain()->add_effect(new ResampleEffect());
	ASSERT_TRUE(resample_effect->set_int("width", 4));
	ASSERT_TRUE(resample_effect->set_int("height", 4));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(data, out_data, size, size);
}

TEST(ResampleEffectTest, UpscaleByTwoGetsCorrectPixelCenters) {
	const int size = 5;

	float data[size * size] = {
		0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0,
	};
	float expected_data[size * size * 4], out_data[size * size * 4];

	for (int y = 0; y < size * 2; ++y) {
		for (int x = 0; x < size * 2; ++x) {
			float weight = lanczos((x - size + 0.5f) * 0.5f, 3.0f);
			weight *= lanczos((y - size + 0.5f) * 0.5f, 3.0f);
			expected_data[y * (size * 2) + x] = weight;
		}
	}

	EffectChainTester tester(nullptr, size * 2, size * 2, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	FlatInput *input = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, size, size);
	input->set_pixel_data(data);
	tester.get_chain()->add_input(input);

	Effect *resample_effect = tester.get_chain()->add_effect(new ResampleEffect());
	ASSERT_TRUE(resample_effect->set_int("width", size * 2));
	ASSERT_TRUE(resample_effect->set_int("height", size * 2));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, size * 2, size * 2);
}

TEST(ResampleEffectTest, DownscaleByTwoGetsCorrectPixelCenters) {
	const int size = 5;

	// This isn't a perfect dot, since the Lanczos filter has a slight
	// sharpening effect; the most important thing is that we have kept
	// the texel center right (everything is nicely symmetric).
	// The approximate magnitudes have been checked against ImageMagick.
	float expected_data[size * size] = {
		 0.0045, -0.0067, -0.0599, -0.0067,  0.0045,
		-0.0067,  0.0100,  0.0892,  0.0100, -0.0067,
		-0.0599,  0.0890,  0.7925,  0.0892, -0.0599,
		-0.0067,  0.0100,  0.0890,  0.0100, -0.0067,
		 0.0045, -0.0067, -0.0599, -0.0067,  0.0045,
	};
	float data[size * size * 4], out_data[size * size];

	for (int y = 0; y < size * 2; ++y) {
		for (int x = 0; x < size * 2; ++x) {
			float weight = lanczos((x - size + 0.5f) * 0.5f, 3.0f);
			weight *= lanczos((y - size + 0.5f) * 0.5f, 3.0f);
			data[y * (size * 2) + x] = weight;
		}
	}

	EffectChainTester tester(nullptr, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	FlatInput *input = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, size * 2, size * 2);
	input->set_pixel_data(data);
	tester.get_chain()->add_input(input);

	Effect *resample_effect = tester.get_chain()->add_effect(new ResampleEffect());
	ASSERT_TRUE(resample_effect->set_int("width", size));
	ASSERT_TRUE(resample_effect->set_int("height", size));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, size, size);
}

TEST(ResampleEffectTest, UpscaleByThreeGetsCorrectPixelCenters) {
	const int size = 5;

	float data[size * size] = {
		0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0,
	};
	float out_data[size * size * 9];

	EffectChainTester tester(nullptr, size * 3, size * 3, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	FlatInput *input = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, size, size);
	input->set_pixel_data(data);
	tester.get_chain()->add_input(input);

	Effect *resample_effect = tester.get_chain()->add_effect(new ResampleEffect());
	ASSERT_TRUE(resample_effect->set_int("width", size * 3));
	ASSERT_TRUE(resample_effect->set_int("height", size * 3));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	// We only bother checking that the middle pixel is still correct,
	// and that symmetry holds. Note that the middle weight in practice
	// becomes something like 0.99999 due to the normalization
	// (some supposedly zero weights become 1e-6 or so), and then after
	// squaring, the error compounds. Ironically, less texture precision
	// here will give a more accurate result, since the weight can get
	// rounded towards 1.0.
	EXPECT_NEAR(1.0, out_data[7 * (size * 3) + 7], 1e-3);
	for (unsigned y = 0; y < size * 3; ++y) {
		for (unsigned x = 0; x < size * 3; ++x) {
			EXPECT_NEAR(out_data[y * (size * 3) + x], out_data[(size * 3 - y - 1) * (size * 3) + x], 1e-6);
			EXPECT_NEAR(out_data[y * (size * 3) + x], out_data[y * (size * 3) + (size * 3 - x - 1)], 1e-6);
		}
	}
}

TEST(ResampleEffectTest, HeavyResampleGetsSumRight) {
	// Do only one resample pass, more specifically the last one, which goes to
	// our fp32 output. This allows us to analyze the precision without intermediate
	// fp16 rounding.
	const int swidth = 1, sheight = 1280;
	const int dwidth = 1, dheight = 64;

	float data[swidth * sheight], out_data[dwidth * dheight], expected_data[dwidth * dheight];
	for (int y = 0; y < sheight; ++y) {
		for (int x = 0; x < swidth; ++x) {
			data[y * swidth + x] = 1.0f;
		}
	}
	for (int y = 0; y < dheight; ++y) {
		for (int x = 0; x < dwidth; ++x) {
			expected_data[y * dwidth + x] = 1.0f;
		}
	}

	EffectChainTester tester(nullptr, dwidth, dheight, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR, GL_RGBA32F);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	FlatInput *input = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, swidth, sheight);
	input->set_pixel_data(data);

	tester.get_chain()->add_input(input);
	Effect *resample_effect = tester.get_chain()->add_effect(new ResampleEffect());
	ASSERT_TRUE(resample_effect->set_int("width", dwidth));
	ASSERT_TRUE(resample_effect->set_int("height", dheight));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	// Require that we are within 10-bit accuracy. Note that this limit is for
	// one pass only, but the limit is tight enough that it should be good enough
	// for 10-bit accuracy even after two passes.
	expect_equal(expected_data, out_data, dwidth, dheight, 0.12 / 1023.0);
}

TEST(ResampleEffectTest, ReadWholePixelFromLeft) {
	const int size = 5;

	float data[size * size] = {
		0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0,
	};
	float expected_data[size * size] = {
		0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0,
	};
	float out_data[size * size];

	EffectChainTester tester(data, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *resample_effect = tester.get_chain()->add_effect(new ResampleEffect());
	ASSERT_TRUE(resample_effect->set_int("width", size));
	ASSERT_TRUE(resample_effect->set_int("height", size));
	ASSERT_TRUE(resample_effect->set_float("left", 1.0f));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, size, size);
}

TEST(ResampleEffectTest, ReadQuarterPixelFromLeft) {
	const int size = 5;

	float data[size * size] = {
		0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0,
	};

	float expected_data[size * size] = {
		0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0,

		// sin(x*pi)/(x*pi) * sin(x*pi/3)/(x*pi/3) for
		// x = -1.75, -0.75, 0.25, 1.25, 2.25.
		// Note that the weight is mostly on the left side.
		-0.06779, 0.27019, 0.89007, -0.13287, 0.03002,

		0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0,
	};
	float out_data[size * size];

	EffectChainTester tester(data, size, size, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *resample_effect = tester.get_chain()->add_effect(new ResampleEffect());
	ASSERT_TRUE(resample_effect->set_int("width", size));
	ASSERT_TRUE(resample_effect->set_int("height", size));
	ASSERT_TRUE(resample_effect->set_float("left", 0.25f));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, size, size);
}

TEST(ResampleEffectTest, ReadQuarterPixelFromTop) {
	const int width = 3;
	const int height = 5;

	float data[width * height] = {
		0.0, 0.0, 0.0,
		0.0, 0.0, 0.0,
		1.0, 0.0, 0.0,
		0.0, 0.0, 0.0,
		0.0, 0.0, 0.0,
	};

	// See ReadQuarterPixelFromLeft for explanation of the data.
	float expected_data[width * height] = {
		-0.06779, 0.0, 0.0,
		 0.27019, 0.0, 0.0,
		 0.89007, 0.0, 0.0,
		-0.13287, 0.0, 0.0,
		 0.03002, 0.0, 0.0,
	};
	float out_data[width * height];

	EffectChainTester tester(data, width, height, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *resample_effect = tester.get_chain()->add_effect(new ResampleEffect());
	ASSERT_TRUE(resample_effect->set_int("width", width));
	ASSERT_TRUE(resample_effect->set_int("height", height));
	ASSERT_TRUE(resample_effect->set_float("top", 0.25f));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, width, height);
}

TEST(ResampleEffectTest, ReadHalfPixelFromLeftAndScale) {
	const int src_width = 4;
	const int dst_width = 8;

	float data[src_width * 1] = {
		1.0, 2.0, 3.0, 4.0,
	};
	float expected_data[dst_width * 1] = {
		// Empirical; the real test is that we are the same for 0.499 and 0.501.
		1.1553, 1.7158, 2.2500, 2.7461, 3.2812, 3.8418, 4.0703, 4.0508
	};
	float out_data[dst_width * 1];

	EffectChainTester tester(nullptr, dst_width, 1, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);

	ImageFormat format;
	format.color_space = COLORSPACE_sRGB;
	format.gamma_curve = GAMMA_LINEAR;

	FlatInput *input = new FlatInput(format, FORMAT_GRAYSCALE, GL_FLOAT, src_width, 1);
	input->set_pixel_data(data);
	tester.get_chain()->add_input(input);

	Effect *resample_effect = tester.get_chain()->add_effect(new ResampleEffect());
	ASSERT_TRUE(resample_effect->set_int("width", dst_width));
	ASSERT_TRUE(resample_effect->set_int("height", 1));

	// Check that we are (almost) the same no matter the rounding.
	ASSERT_TRUE(resample_effect->set_float("left", 0.499f));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);
	expect_equal(expected_data, out_data, dst_width, 1, 1.5f / 255.0f, 0.4f / 255.0f);

	ASSERT_TRUE(resample_effect->set_float("left", 0.501f));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);
	expect_equal(expected_data, out_data, dst_width, 1, 1.5f / 255.0f, 0.4f / 255.0f);
}

TEST(ResampleEffectTest, Zoom) {
	const int width = 5;
	const int height = 3;

	float data[width * height] = {
		0.0, 0.0, 0.0, 0.0, 0.0,
		0.2, 0.4, 0.6, 0.4, 0.2,
		0.0, 0.0, 0.0, 0.0, 0.0,
	};
	float expected_data[width * height] = {
		0.0, 0.0,    0.0, 0.0,    0.0,
		0.4, 0.5396, 0.6, 0.5396, 0.4,
		0.0, 0.0,    0.0, 0.0,    0.0,
	};
	float out_data[width * height];

	EffectChainTester tester(data, width, height, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *resample_effect = tester.get_chain()->add_effect(new ResampleEffect());
	ASSERT_TRUE(resample_effect->set_int("width", width));
	ASSERT_TRUE(resample_effect->set_int("height", height));
	ASSERT_TRUE(resample_effect->set_float("zoom_x", 2.0f));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, width, height);
}

TEST(ResampleEffectTest, VerticalZoomFromTop) {
	const int width = 5;
	const int height = 5;

	float data[width * height] = {
		0.2, 0.4, 0.6, 0.4, 0.2,
		0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0, 0.0,
	};

	// Largely empirical data; the main point is that the top line
	// is unchanged, since that's our zooming point.
	float expected_data[width * height] = {
		 0.2000,  0.4000,  0.6000,  0.4000,  0.2000,
		 0.1389,  0.2778,  0.4167,  0.2778,  0.1389,
		 0.0600,  0.1199,  0.1798,  0.1199,  0.0600,
		 0.0000,  0.0000,  0.0000,  0.0000,  0.0000,
		-0.0229, -0.0459, -0.0688, -0.0459, -0.0229,
	};
	float out_data[width * height];

	EffectChainTester tester(data, width, height, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *resample_effect = tester.get_chain()->add_effect(new ResampleEffect());
	ASSERT_TRUE(resample_effect->set_int("width", width));
	ASSERT_TRUE(resample_effect->set_int("height", height));
	ASSERT_TRUE(resample_effect->set_float("zoom_y", 3.0f));
	ASSERT_TRUE(resample_effect->set_float("zoom_center_y", 0.5f / height));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, width, height);
}

TEST(ResampleEffectTest, Precision) {
	const int size = 1920;  // Difficult non-power-of-two size.
	const int offset = 5;

	// Deliberately put the data of interest very close to the right,
	// where texture coordinates are farther from 0 and thus less precise.
	float data[size * 2] = {0};
	data[size - offset] = 1.0f;
	float expected_data[size * 2] = {0};
	for (int x = 0; x < size * 2; ++x) {
		expected_data[x] = lanczos((x - (size - 2 * offset + 1) + 0.5f) * 0.5f, 3.0f);
	}
	float out_data[size * 2];

	EffectChainTester tester(data, size * 2, 1, FORMAT_GRAYSCALE, COLORSPACE_sRGB, GAMMA_LINEAR);
	Effect *resample_effect = tester.get_chain()->add_effect(new ResampleEffect());
	ASSERT_TRUE(resample_effect->set_int("width", size * 2));
	ASSERT_TRUE(resample_effect->set_int("height", 1));
	ASSERT_TRUE(resample_effect->set_float("zoom_x", 2.0f));
	tester.run(out_data, GL_RED, COLORSPACE_sRGB, GAMMA_LINEAR);

	expect_equal(expected_data, out_data, size, 1);
}

#ifdef HAVE_BENCHMARK
template<> inline uint8_t from_fp32<uint8_t>(float x) { return lrintf(x * 255.0f); }

template<class T>
void BM_ResampleEffect(benchmark::State &state, GammaCurve gamma_curve, GLenum output_format, const std::string &shader_type)
{
	DisableComputeShadersTemporarily disabler(shader_type == "fragment");
	if (disabler.should_skip(&state)) return;

	unsigned in_width = state.range(0), in_height = state.range(1);
	unsigned out_width = state.range(2), out_height = state.range(3);

	unique_ptr<T[]> data(new T[in_width * in_height * 4]);
	unique_ptr<T[]> out_data(new T[out_width * out_height * 4]);

	for (unsigned i = 0; i < in_width * in_height * 4; ++i) {
		data[i] = from_fp32<T>(rand() / (RAND_MAX + 1.0));
	}

	EffectChainTester tester(nullptr, out_width, out_height, FORMAT_BGRA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, gamma_curve, output_format);
	tester.add_input(data.get(), FORMAT_BGRA_POSTMULTIPLIED_ALPHA, COLORSPACE_sRGB, gamma_curve, in_width, in_height);
	Effect *resample_effect = tester.get_chain()->add_effect(new ResampleEffect());

	ASSERT_TRUE(resample_effect->set_int("width", out_width));
	ASSERT_TRUE(resample_effect->set_int("height", out_height));

	tester.benchmark(state, out_data.get(), GL_BGRA, COLORSPACE_sRGB, gamma_curve, OUTPUT_ALPHA_FORMAT_PREMULTIPLIED);
}

void BM_ResampleEffectHalf(benchmark::State &state, GammaCurve gamma_curve, const std::string &shader_type)
{
	BM_ResampleEffect<fp16_int_t>(state, gamma_curve, GL_RGBA16F, shader_type);
}

void BM_ResampleEffectInt8(benchmark::State &state, GammaCurve gamma_curve, const std::string &shader_type)
{
	BM_ResampleEffect<uint8_t>(state, gamma_curve, GL_RGBA8, shader_type);
}

BENCHMARK_CAPTURE(BM_ResampleEffectInt8, Int8Upscale, GAMMA_REC_709, "fragment")->Args({640, 360, 1280, 720})->Args({320, 180, 1280, 720})->Args({321, 181, 1280, 720})->UseRealTime()->Unit(benchmark::kMicrosecond);
BENCHMARK_CAPTURE(BM_ResampleEffectHalf, Float16Upscale, GAMMA_LINEAR, "fragment")->Args({640, 360, 1280, 720})->Args({320, 180, 1280, 720})->Args({321, 181, 1280, 720})->UseRealTime()->Unit(benchmark::kMicrosecond);
BENCHMARK_CAPTURE(BM_ResampleEffectInt8, Int8Downscale, GAMMA_REC_709, "fragment")->Args({1280, 720, 640, 360})->Args({1280, 720, 320, 180})->Args({1280, 720, 321, 181})->UseRealTime()->Unit(benchmark::kMicrosecond);
BENCHMARK_CAPTURE(BM_ResampleEffectHalf, Float16Downscale, GAMMA_LINEAR, "fragment")->Args({1280, 720, 640, 360})->Args({1280, 720, 320, 180})->Args({1280, 720, 321, 181})->UseRealTime()->Unit(benchmark::kMicrosecond);

void BM_ComputeBilinearScalingWeights(benchmark::State &state)
{
	constexpr unsigned src_size = 1280;
	constexpr unsigned dst_size = 35;
	int old_precision = movit_texel_subpixel_precision;
	movit_texel_subpixel_precision = 64;  // To get consistent results across GPUs; this is a CPU test.

	// One iteration warmup to make sure the Lanczos table is computed.
	calculate_bilinear_scaling_weights(src_size, dst_size, 0.999f, 0.0f);

	for (auto _ : state) {
		ScalingWeights weights = calculate_bilinear_scaling_weights(src_size, dst_size, 0.999f, 0.0f);
	}

	movit_texel_subpixel_precision = old_precision;
}
BENCHMARK(BM_ComputeBilinearScalingWeights)->Unit(benchmark::kMicrosecond);

#endif

}  // namespace movit
