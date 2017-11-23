#ifndef _MOVIT_FFT_INPUT_H
#define _MOVIT_FFT_INPUT_H 1

// FFTInput is used by FFTConvolutionEffect to send in the FFTed version of a
// mostly static, one-channel data set, typically the convolution kernel
// with some zero padding.
//
// Since the kernel is typically small and unlikely to change often,
// it will be faster to FFT it once on the CPU (using the excellent FFTW3
// library) and keep it in a texture, rather than FFT-ing it over and over on
// the GPU. (We do not currently support caching Movit intermediates between
// frames.) As an extra bonus, we can then do it in double precision and round
// precisely to fp16 afterwards.
//
// This class is tested as part of by FFTConvolutionEffectTest.

#include <epoxy/gl.h>
#include <assert.h>
#include <string>

#include "effect.h"
#include "effect_chain.h"
#include "image_format.h"
#include "input.h"

namespace movit {

class ResourcePool;

class FFTInput : public Input {
public:
	FFTInput(unsigned width, unsigned height);
	~FFTInput();

	std::string effect_type_id() const override { return "FFTInput"; }
	std::string output_fragment_shader() override;

	// FFTs the data and uploads the texture if it has changed since last time.
	void set_gl_state(GLuint glsl_program_num, const std::string& prefix, unsigned *sampler_num) override;

	unsigned get_width() const override { return fft_width; }
	unsigned get_height() const override { return fft_height; }

	// Strictly speaking, FFT data doesn't have any colorspace or gamma;
	// these values are the Movit standards for “do nothing”.
	Colorspace get_color_space() const override { return COLORSPACE_sRGB; }
	GammaCurve get_gamma_curve() const override { return GAMMA_LINEAR; }
	AlphaHandling alpha_handling() const override { return INPUT_AND_OUTPUT_PREMULTIPLIED_ALPHA; }
	bool is_single_texture() const override { return true; }
	bool can_output_linear_gamma() const override { return true; }
	bool can_supply_mipmaps() const override { return false; }

	// Tells the input where to fetch the actual pixel data. Note that if you change
	// this data, you must either call set_pixel_data() again (using the same pointer
	// is fine), or invalidate_pixel_data(). Otherwise, the FFT won't be recalculated,
	// and the texture won't be re-uploaded on subsequent frames.
	void set_pixel_data(const float *pixel_data)
	{
		this->pixel_data = pixel_data;
		invalidate_pixel_data();
	}

	void invalidate_pixel_data();

	void inform_added(EffectChain *chain) override
	{
		resource_pool = chain->get_resource_pool();
	}

	bool set_int(const std::string& key, int value) override;

private:
	GLuint texture_num;
	int fft_width, fft_height;
	unsigned convolve_width, convolve_height;
	const float *pixel_data;
	ResourcePool *resource_pool;
	GLint uniform_tex;
};

}  // namespace movit

#endif // !defined(_MOVIT_FFT_INPUT_H)
