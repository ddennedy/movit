#ifndef _MOVIT_YCBCR_INPUT_H
#define _MOVIT_YCBCR_INPUT_H 1

// YCbCrInput is for handling Y'CbCr (also sometimes, usually rather
// imprecisely, called “YUV”), which is typically what you get from a video
// decoder. It supports these formats:
//
//   * 8-bit planar Y'CbCr, possibly subsampled (e.g. 4:2:0).
//   * 8-bit semiplanar Y'CbCr (Y' in one plane, CbCr in another),
//     possibly subsampled.
//   * 8-bit interleaved (chunked) Y'CbCr, no subsampling (4:4:4 only).
//   * All of the above in 10- and 12-bit versions, where each sample is
//     stored in a 16-bit int (so the 6 or 4 top bits are wasted).
//   * 10-bit interleaved (chunked) Y'CbCr packed into 32-bit words
//     (10:10:10:2), no subsampling (4:4:4 only).
//
// For the planar and semiplanar cases, it upsamples planes as needed, using
// the default linear upsampling OpenGL gives you. Note that YCbCr422InterleavedInput
// supports the important special case of 8-bit 4:2:2 interleaved.

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

// Whether the data is planar (Y', Cb and Cr in one texture each) or not.
enum YCbCrInputSplitting {
	// The standard, default case; Y', Cb and Cr in one texture each.
	YCBCR_INPUT_PLANAR,

	// Y' in one texture, and then Cb and Cr interleaved in one texture.
	// In particular, this is a superset of the relatively popular NV12 mode.
	// If you specify this mode, the “Cr” pointer texture will be unused
	// (the ”Cb” texture contains both).
	YCBCR_INPUT_SPLIT_Y_AND_CBCR,

	// Y', Cb and Cr interleaved in the same texture (the “Y” texture;
	// “Cb” and “Cr” are unused). This means you cannot have any subsampling;
	// 4:4:4 only.
	YCBCR_INPUT_INTERLEAVED,
};

class YCbCrInput : public Input {
public:
	// Type can be GL_UNSIGNED_BYTE for 8-bit, GL_UNSIGNED_SHORT for 10- or 12-bit
	// (or 8-bit, although that's a bit useless), or GL_UNSIGNED_INT_2_10_10_10_REV
	// for 10-bit (YCBCR_INPUT_INTERLEAVED only).
	YCbCrInput(const ImageFormat &image_format,
	           const YCbCrFormat &ycbcr_format,
	           unsigned width, unsigned height,
	           YCbCrInputSplitting ycbcr_input_splitting = YCBCR_INPUT_PLANAR,
	           GLenum type = GL_UNSIGNED_BYTE);
	~YCbCrInput();

	std::string effect_type_id() const override { return "YCbCrInput"; }

	bool can_output_linear_gamma() const override { return false; }
	AlphaHandling alpha_handling() const override { return OUTPUT_BLANK_ALPHA; }

	std::string output_fragment_shader() override;

	// Uploads the texture if it has changed since last time.
	void set_gl_state(GLuint glsl_program_num, const std::string& prefix, unsigned *sampler_num) override;

	unsigned get_width() const override { return width; }
	unsigned get_height() const override { return height; }
	Colorspace get_color_space() const override { return image_format.color_space; }
	GammaCurve get_gamma_curve() const override { return image_format.gamma_curve; }
	bool can_supply_mipmaps() const override { return ycbcr_input_splitting == YCBCR_INPUT_INTERLEAVED; }
	bool is_single_texture() const override { return ycbcr_input_splitting == YCBCR_INPUT_INTERLEAVED; }

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
		assert(type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_INT_2_10_10_10_REV);
		assert(channel >= 0 && channel < num_channels);
		this->pixel_data[channel] = pixel_data;
		this->pbos[channel] = pbo;
		invalidate_pixel_data();
	}

	void set_pixel_data(unsigned channel, const uint16_t *pixel_data, GLuint pbo = 0)
	{
		assert(type == GL_UNSIGNED_SHORT);
		assert(channel >= 0 && channel < num_channels);
		this->pixel_data[channel] = reinterpret_cast<const unsigned char *>(pixel_data);
		this->pbos[channel] = pbo;
		invalidate_pixel_data();
	}

	void set_pixel_data(unsigned channel, const uint32_t *pixel_data, GLuint pbo = 0)
	{
		assert(type == GL_UNSIGNED_INT_2_10_10_10_REV);
		assert(channel == 0);
		this->pixel_data[channel] = reinterpret_cast<const unsigned char *>(pixel_data);
		this->pbos[channel] = pbo;
		invalidate_pixel_data();
	}

	void invalidate_pixel_data();

	// Note: Sets pitch to width, so even if your pitch is unchanged,
	// you will need to re-set it after this call.
	void set_width(unsigned width)
	{
		assert(width != 0);
		this->width = width;

		assert(width % ycbcr_format.chroma_subsampling_x == 0);
		pitch[0] = widths[0] = width;
		pitch[1] = widths[1] = width / ycbcr_format.chroma_subsampling_x;
		pitch[2] = widths[2] = width / ycbcr_format.chroma_subsampling_x;
		invalidate_pixel_data();
	}

	void set_height(unsigned height)
	{
		assert(height != 0);
		this->height = height;

		assert(height % ycbcr_format.chroma_subsampling_y == 0);
		heights[0] = height;
		heights[1] = height / ycbcr_format.chroma_subsampling_y;
		heights[2] = height / ycbcr_format.chroma_subsampling_y;
		invalidate_pixel_data();
	}

	void set_pitch(unsigned channel, unsigned pitch)
	{
		assert(pitch != 0);
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

	// You can change the Y'CbCr format freely, also after finalize,
	// although with one limitation: If Cb and Cr come from the same
	// texture and their offsets offsets are the same (ie., within 1e-6)
	// when finalizing, they most continue to be so forever, as this
	// optimization is compiled into the shader.
	//
	// If you change subsampling parameters, you'll need to call
	// set_width() / set_height() again after this.
	void change_ycbcr_format(const YCbCrFormat &ycbcr_format);

	void inform_added(EffectChain *chain) override
	{
		resource_pool = chain->get_resource_pool();
	}

	bool set_int(const std::string& key, int value) override;

private:
	// Release the texture in the given channel if we have any, and it is owned by us.
	void possibly_release_texture(unsigned channel);

	ImageFormat image_format;
	YCbCrFormat ycbcr_format;
	GLuint num_channels;
	YCbCrInputSplitting ycbcr_input_splitting;
	int needs_mipmaps;  // Only allowed if ycbcr_input_splitting == YCBCR_INPUT_INTERLEAVED.
	GLenum type;
	GLuint pbos[3], texture_num[3];
	GLint uniform_tex_y, uniform_tex_cb, uniform_tex_cr;
	Eigen::Matrix3d uniform_ycbcr_matrix;
	float uniform_offset[3];
	Point2D uniform_cb_offset, uniform_cr_offset;
	bool cb_cr_offsets_equal;

	unsigned width, height, widths[3], heights[3];
	const unsigned char *pixel_data[3];
	unsigned pitch[3];
	bool owns_texture[3];
	ResourcePool *resource_pool;
};

}  // namespace movit

#endif // !defined(_MOVIT_YCBCR_INPUT_H)
