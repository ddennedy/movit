#ifndef _MOVIT_YCBCR_INPUT_H
#define _MOVIT_YCBCR_INPUT_H 1

// YCbCrInput is for handling planar 8-bit Y'CbCr (also sometimes, usually rather
// imprecisely, called “YUV”), which is typically what you get from a video decoder.
// It upsamples planes as needed, using the default linear upsampling OpenGL gives you.

#include <epoxy/gl.h>
#include <assert.h>
#include <string>

#include "effect.h"
#include "effect_chain.h"
#include "image_format.h"
#include "input.h"
#include "ycbcr.h"

namespace movit {

class ResourcePool;

// Whether the data is fully planar (Y', Cb and Cr in one texture each)
// or not. Note that this input does currently not support fully interleaved
// data (Y', Cb and Cr next to each other), as 4:4:4 interleaved Y'CbCr seems
// to be rare; however, YCbCr422InterleavedInput supports the important special
// case of 4:2:2 interleaved.
enum YCbCrInputSplitting {
	// The standard, default case; Y', Cb and Cr in one texture each.
	YCBCR_INPUT_PLANAR,

	// Y' in one texture, and then Cb and Cr interleaved in one texture.
	// In particular, this is a superset of the relatively popular NV12 mode.
	// If you specify this mode, the “Cr” pointer texture will be unused
	// (the ”Cb” texture contains both).
	YCBCR_INPUT_SPLIT_Y_AND_CBCR,
};

class YCbCrInput : public Input {
public:
	YCbCrInput(const ImageFormat &image_format,
	           const YCbCrFormat &ycbcr_format,
	           unsigned width, unsigned height,
	           YCbCrInputSplitting ycbcr_input_splitting = YCBCR_INPUT_PLANAR);
	~YCbCrInput();

	virtual std::string effect_type_id() const { return "YCbCrInput"; }

	virtual bool can_output_linear_gamma() const { return false; }
	virtual AlphaHandling alpha_handling() const { return OUTPUT_BLANK_ALPHA; }

	std::string output_fragment_shader();

	// Uploads the texture if it has changed since last time.
	void set_gl_state(GLuint glsl_program_num, const std::string& prefix, unsigned *sampler_num);

	unsigned get_width() const { return width; }
	unsigned get_height() const { return height; }
	Colorspace get_color_space() const { return image_format.color_space; }
	GammaCurve get_gamma_curve() const { return image_format.gamma_curve; }
	virtual bool can_supply_mipmaps() const { return false; }

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
	void set_pixel_data(unsigned channel, const unsigned char *pixel_data, GLuint pbo = 0)
	{
		assert(channel >= 0 && channel < num_channels);
		this->pixel_data[channel] = pixel_data;
		this->pbos[channel] = pbo;
		invalidate_pixel_data();
	}

	void invalidate_pixel_data();

	void set_pitch(unsigned channel, unsigned pitch) {
		assert(channel >= 0 && channel < num_channels);
		this->pitch[channel] = pitch;
		invalidate_pixel_data();
	}

	// Tells the input to use the specific OpenGL texture as pixel data for the given
	// channel. The comments on FlatInput::set_texture_num() also apply here, except
	// that this input generally does not use mipmaps.
	void set_texture_num(unsigned channel, GLuint texture_num)
	{
		possibly_release_texture(channel);
		this->texture_num[channel] = texture_num;
		this->owns_texture[channel] = false;
	}

	virtual void inform_added(EffectChain *chain)
	{
		resource_pool = chain->get_resource_pool();
	}

	bool set_int(const std::string& key, int value);

private:
	// Release the texture in the given channel if we have any, and it is owned by us.
	void possibly_release_texture(unsigned channel);

	ImageFormat image_format;
	YCbCrFormat ycbcr_format;
	GLuint num_channels;
	YCbCrInputSplitting ycbcr_input_splitting;
	GLuint pbos[3], texture_num[3];
	GLint uniform_tex_y, uniform_tex_cb, uniform_tex_cr;

	unsigned width, height, widths[3], heights[3];
	const unsigned char *pixel_data[3];
	unsigned pitch[3];
	bool owns_texture[3];
	ResourcePool *resource_pool;
};

}  // namespace movit

#endif // !defined(_MOVIT_YCBCR_INPUT_H)
