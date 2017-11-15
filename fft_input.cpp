#include <string.h>
#include <assert.h>
#include <epoxy/gl.h>
#include <fftw3.h>

#include "effect_util.h"
#include "fp16.h"
#include "fft_input.h"
#include "resource_pool.h"
#include "util.h"

using namespace std;

namespace movit {

FFTInput::FFTInput(unsigned width, unsigned height)
	: texture_num(0),
	  fft_width(width),
	  fft_height(height),
	  convolve_width(width),
	  convolve_height(height),
	  pixel_data(nullptr)
{
	register_int("fft_width", &fft_width);
	register_int("fft_height", &fft_height);
	register_uniform_sampler2d("tex", &uniform_tex);
}

FFTInput::~FFTInput()
{
	if (texture_num != 0) {
		resource_pool->release_2d_texture(texture_num);
	}
}

void FFTInput::set_gl_state(GLuint glsl_program_num, const string& prefix, unsigned *sampler_num)
{
	glActiveTexture(GL_TEXTURE0 + *sampler_num);
	check_error();

	if (texture_num == 0) {
		assert(pixel_data != nullptr);

		// Do the FFT. Our FFTs should typically be small enough and
		// the data changed often enough that FFTW_ESTIMATE should be
		// quite OK. Otherwise, we'd need to worry about caching these
		// plans (possibly including FFTW wisdom).
		fftw_complex *in = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * fft_width * fft_height);
		fftw_complex *out = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * fft_width * fft_height);
		fftw_plan p = fftw_plan_dft_2d(fft_height, fft_width, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

		// Zero pad.
		for (int i = 0; i < fft_height * fft_width; ++i) {
			in[i][0] = 0.0;
			in[i][1] = 0.0;
		}
		for (unsigned y = 0; y < convolve_height; ++y) {
			for (unsigned x = 0; x < convolve_width; ++x) {
				int i = y * fft_width + x;
				in[i][0] = pixel_data[y * convolve_width + x];
				in[i][1] = 0.0;
			}
		}

		fftw_execute(p);

		// Convert to fp16.
		fp16_int_t *kernel = new fp16_int_t[fft_width * fft_height * 2];
		for (int i = 0; i < fft_width * fft_height; ++i) {
			kernel[i * 2 + 0] = fp32_to_fp16(out[i][0]);
			kernel[i * 2 + 1] = fp32_to_fp16(out[i][1]);
		}

		// (Re-)upload the texture.
		texture_num = resource_pool->create_2d_texture(GL_RG16F, fft_width, fft_height);
		glBindTexture(GL_TEXTURE_2D, texture_num);
		check_error();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		check_error();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		check_error();
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		check_error();
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, fft_width, fft_height, GL_RG, GL_HALF_FLOAT, kernel);
		check_error();
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		check_error();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		check_error();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		check_error();

		fftw_free(in);
		fftw_free(out);
		delete[] kernel;
	} else {
		glBindTexture(GL_TEXTURE_2D, texture_num);
		check_error();
	}

	// Bind it to a sampler.
	uniform_tex = *sampler_num;
	++*sampler_num;
}

string FFTInput::output_fragment_shader()
{
	return string("#define FIXUP_SWAP_RB 0\n#define FIXUP_RED_TO_GRAYSCALE 0\n") +
		read_file("flat_input.frag");
}

void FFTInput::invalidate_pixel_data()
{
	if (texture_num != 0) {
		resource_pool->release_2d_texture(texture_num);
		texture_num = 0;
	}
}

bool FFTInput::set_int(const std::string& key, int value)
{
	if (key == "needs_mipmaps") {
		// We cannot supply mipmaps; it would not make any sense for FFT data.
		return (value == 0);
	}
	if (key == "fft_width") {
		if (value < int(convolve_width)) {
			return false;
		}
		invalidate_pixel_data();
	}
	if (key == "fft_height") {
		if (value < int(convolve_height)) {
			return false;
		}
		invalidate_pixel_data();
	}
	return Effect::set_int(key, value);
}

}  // namespace movit
