#ifndef _MOVIT_FLAT_INPUT_H
#define _MOVIT_FLAT_INPUT_H 1

#include <epoxy/gl.h>
#include <assert.h>
#include <string>

#include "effect.h"
#include "effect_chain.h"
#include "fp16.h"
#include "image_format.h"
#include "input.h"

namespace movit {

class ResourcePool;

// A FlatInput is the normal, “classic” case of an input, where everything
// comes from a single 2D array with chunky pixels.
class FlatInput : public Input {
public:
	FlatInput(ImageFormat format, MovitPixelFormat pixel_format, GLenum type, unsigned width, unsigned height);
	~FlatInput();

	std::string effect_type_id() const override { return "FlatInput"; }

	bool can_output_linear_gamma() const override {
		// On desktop OpenGL, there's also GL_SLUMINANCE8 which could give us
		// support for single-channel sRGB decoding, but it's not supported
		// on GLES, and we're already actively rewriting single-channel inputs
		// to GL_RED (even on desktop), so we stick to 3- and 4-channel inputs.
		return (type == GL_UNSIGNED_BYTE &&
			(pixel_format == FORMAT_RGB ||
			 pixel_format == FORMAT_RGBA_POSTMULTIPLIED_ALPHA) &&
		        (image_format.gamma_curve == GAMMA_LINEAR ||
		         image_format.gamma_curve == GAMMA_sRGB));
	}
	AlphaHandling alpha_handling() const override {
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

	std::string output_fragment_shader() override;

	// Uploads the texture if it has changed since last time.
	void set_gl_state(GLuint glsl_program_num, const std::string& prefix, unsigned *sampler_num) override;

	unsigned get_width() const override { return width; }
	unsigned get_height() const override { return height; }
	Colorspace get_color_space() const override { return image_format.color_space; }
	GammaCurve get_gamma_curve() const override { return image_format.gamma_curve; }
	bool is_single_texture() const override { return true; }

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

	// Note: Sets pitch to width, so even if your pitch is unchanged,
	// you will need to re-set it after this call.
	void set_width(unsigned width)
	{
		assert(width != 0);
		this->pitch = this->width = width;
		invalidate_pixel_data();
	}

	void set_height(unsigned height)
	{
		assert(height != 0);
		this->height = height;
		invalidate_pixel_data();
	}

	void set_pitch(unsigned pitch) {
		assert(pitch != 0);
		this->pitch = pitch;
		invalidate_pixel_data();
	}

	// Tells the input to use the specific OpenGL texture as pixel data.
	// This is useful if you want to share the same texture between multiple
	// EffectChain instances, or if you somehow can get the data into a texture more
	// efficiently than through a normal upload (e.g. a video codec decoding straight
	// into a texture). Note that you are responsible for setting the right sampler
	// parameters (e.g. clamp-to-edge) yourself, as well as generate any mipmaps
	// if they are needed.
	//
	// NOTE: The input does not take ownership of this texture; you are responsible
	// for releasing it yourself. In particular, if you call invalidate_pixel_data()
	// or anything calling it, the texture will silently be removed from the input.
	//
	// NOTE: Doing this in a situation where can_output_linear_gamma() is true
	// can yield unexpected results, as the downstream effect can expect the texture
	// to be uploaded with the sRGB flag on.
	void set_texture_num(GLuint texture_num)
	{
		possibly_release_texture();
		this->texture_num = texture_num;
		this->owns_texture = false;
	}

	void inform_added(EffectChain *chain) override
	{
		resource_pool = chain->get_resource_pool();
	}

private:
	// Release the texture if we have any, and it is owned by us.
	void possibly_release_texture();

	ImageFormat image_format;
	MovitPixelFormat pixel_format;
	GLenum type;
	GLuint pbo, texture_num;
	int output_linear_gamma, needs_mipmaps;
	unsigned width, height, pitch;
	bool owns_texture;
	const void *pixel_data;
	ResourcePool *resource_pool;
	bool fixup_swap_rb, fixup_red_to_grayscale;
	GLint uniform_tex;
};

}  // namespace movit

#endif // !defined(_MOVIT_FLAT_INPUT_H)
