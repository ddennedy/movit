#ifndef _MOVIT_FLAT_INPUT_H
#define _MOVIT_FLAT_INPUT_H 1

#include <epoxy/gl.h>
#include <assert.h>
#include <string>

#include "effect.h"
#include "effect_chain.h"
#include "fp16.h"
#include "image_format.h"
#include "init.h"
#include "input.h"

namespace movit {

class ResourcePool;

// A FlatInput is the normal, “classic” case of an input, where everything
// comes from a single 2D array with chunky pixels.
class FlatInput : public Input {
public:
	FlatInput(ImageFormat format, MovitPixelFormat pixel_format, GLenum type, unsigned width, unsigned height);
	~FlatInput();

	virtual std::string effect_type_id() const { return "FlatInput"; }

	virtual bool can_output_linear_gamma() const {
		// On desktop OpenGL, there's also GL_SLUMINANCE8 which could give us
		// support for single-channel sRGB decoding, but it's not supported
		// on GLES, and we're already actively rewriting single-channel inputs
		// to GL_RED (even on desktop), so we stick to 3- and 4-channel inputs.
		return (movit_srgb_textures_supported &&
		        type == GL_UNSIGNED_BYTE &&
			(pixel_format == FORMAT_RGB ||
			 pixel_format == FORMAT_RGBA_POSTMULTIPLIED_ALPHA) &&
		        (image_format.gamma_curve == GAMMA_LINEAR ||
		         image_format.gamma_curve == GAMMA_sRGB));
	}
	virtual AlphaHandling alpha_handling() const {
		switch (pixel_format) {
		case FORMAT_RGBA_PREMULTIPLIED_ALPHA:
			return INPUT_AND_OUTPUT_PREMULTIPLIED_ALPHA;
		case FORMAT_RGBA_POSTMULTIPLIED_ALPHA:
			return OUTPUT_POSTMULTIPLIED_ALPHA;
		case FORMAT_R:
		case FORMAT_RG:
		case FORMAT_RGB:
			return OUTPUT_BLANK_ALPHA;
		default:
			assert(false);
		}
	}

	std::string output_fragment_shader();

	// Uploads the texture if it has changed since last time.
	void set_gl_state(GLuint glsl_program_num, const std::string& prefix, unsigned *sampler_num);

	unsigned get_width() const { return width; }
	unsigned get_height() const { return height; }
	Colorspace get_color_space() const { return image_format.color_space; }
	GammaCurve get_gamma_curve() const { return image_format.gamma_curve; }
	virtual bool is_single_texture() const { return true; }

	// Tells the input where to fetch the actual pixel data. Note that if you change
	// this data, you must either call set_pixel_data() again (using the same pointer
	// is fine), or invalidate_pixel_data(). Otherwise, the texture won't be re-uploaded
	// on subsequent frames.
	//
	// The data can either be a regular pointer (if pbo==0), or a byte offset
	// into a PBO. The latter will allow you to start uploading the texture data
	// asynchronously to the GPU, if you have any CPU-intensive work between the
	// call to set_pixel_data() and the actual rendering. In either case,
	// the pointer (and PBO, if set) has to be valid at the time of the render call.
	void set_pixel_data(const unsigned char *pixel_data, GLuint pbo = 0)
	{
		assert(this->type == GL_UNSIGNED_BYTE);
		this->pixel_data = pixel_data;
		this->pbo = pbo;
		invalidate_pixel_data();
	}

	void set_pixel_data(const unsigned short *pixel_data, GLuint pbo = 0)
	{
		assert(this->type == GL_UNSIGNED_SHORT);
		this->pixel_data = pixel_data;
		this->pbo = pbo;
		invalidate_pixel_data();
	}

	void set_pixel_data_fp16(const fp16_int_t *pixel_data, GLuint pbo = 0)
	{
		assert(this->type == GL_HALF_FLOAT);
		this->pixel_data = pixel_data;
		this->pbo = pbo;
		invalidate_pixel_data();
	}

	void set_pixel_data(const float *pixel_data, GLuint pbo = 0)
	{
		assert(this->type == GL_FLOAT);
		this->pixel_data = pixel_data;
		this->pbo = pbo;
		invalidate_pixel_data();
	}

	void invalidate_pixel_data();

	void set_pitch(unsigned pitch) {
		this->pitch = pitch;
		invalidate_pixel_data();
	}

	virtual void inform_added(EffectChain *chain)
	{
		resource_pool = chain->get_resource_pool();
	}

private:
	ImageFormat image_format;
	MovitPixelFormat pixel_format;
	GLenum type;
	GLuint pbo, texture_num;
	int output_linear_gamma, needs_mipmaps;
	unsigned width, height, pitch;
	const void *pixel_data;
	ResourcePool *resource_pool;
	bool fixup_swap_rb, fixup_red_to_grayscale;
	GLint uniform_tex;
};

}  // namespace movit

#endif // !defined(_MOVIT_FLAT_INPUT_H)
