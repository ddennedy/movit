#ifndef _MOVIT_YCBCR_INPUT_H
#define _MOVIT_YCBCR_INPUT_H 1

// YCbCrInput is for handling planar 8-bit Y'CbCr (also sometimes, usually rather
// imprecisely, called “YUV”), which is typically what you get from a video decoder.
// It upsamples planes as needed, using the default linear upsampling OpenGL gives you.

#include <GL/glew.h>
#include <assert.h>
#include <string>

#include "effect.h"
#include "effect_chain.h"
#include "image_format.h"
#include "input.h"

namespace movit {

class ResourcePool;

struct YCbCrFormat {
	// Which formula for Y' to use.
	YCbCrLumaCoefficients luma_coefficients;

	// If true, assume Y'CbCr coefficients are full-range, ie. go from 0 to 255
	// instead of the limited 220/225 steps in classic MPEG. For instance,
	// JPEG uses the Rec. 601 luma coefficients, but full range.
	bool full_range;

	// Sampling factors for chroma components. For no subsampling (4:4:4),
	// set both to 1.
	unsigned chroma_subsampling_x, chroma_subsampling_y;

	// Positioning of the chroma samples. MPEG-1 and JPEG is (0.5, 0.5);
	// MPEG-2 and newer typically are (0.0, 0.5).
	float cb_x_position, cb_y_position;
	float cr_x_position, cr_y_position;
};

class YCbCrInput : public Input {
public:
	YCbCrInput(const ImageFormat &image_format,
	           const YCbCrFormat &ycbcr_format,
	           unsigned width, unsigned height);
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
		assert(channel >= 0 && channel < 3);
		this->pixel_data[channel] = pixel_data;
		this->pbos[channel] = pbo;
		invalidate_pixel_data();
	}

	void invalidate_pixel_data();

	void set_pitch(unsigned channel, unsigned pitch) {
		assert(channel >= 0 && channel < 3);
		this->pitch[channel] = pitch;
		invalidate_pixel_data();
	}

	virtual void inform_added(EffectChain *chain)
	{
		resource_pool = chain->get_resource_pool();
	}

private:
	ImageFormat image_format;
	YCbCrFormat ycbcr_format;
	GLuint pbos[3], texture_num[3];

	int needs_mipmaps;

	unsigned width, height, widths[3], heights[3];
	const unsigned char *pixel_data[3];
	unsigned pitch[3];
	ResourcePool *resource_pool;
};

}  // namespace movit

#endif // !defined(_MOVIT_YCBCR_INPUT_H)
