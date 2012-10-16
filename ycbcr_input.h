#ifndef _YCBCR_INPUT_H
#define _YCBCR_INPUT_H 1

// YCbCrInput is for handling planar 8-bit Y'CbCr (also sometimes, usually rather
// imprecisely, called “YUV”), which is typically what you get from a video decoder.
// It upsamples planes as needed, using the default linear upsampling OpenGL gives you.

#include "input.h"

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

	// Create the texture itself. We cannot do this in the constructor,
	// because we don't necessarily know all the settings (sRGB texture,
	// mipmap generation) at that point.
	void finalize();

	virtual bool can_output_linear_gamma() const { return false; }

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
	void set_pixel_data(unsigned channel, const unsigned char *pixel_data)
	{
		assert(channel >= 0 && channel < 3);
		this->pixel_data[channel] = pixel_data;
		invalidate_pixel_data();
	}

	void invalidate_pixel_data()
	{
		needs_update = true;
	}

	void set_pitch(unsigned channel, unsigned pitch) {
		assert(channel >= 0 && channel < 3);
		if (this->pitch[channel] != pitch) {
			this->pitch[channel] = pitch;
			needs_pbo_recreate = true;
		}
	}

private:
	ImageFormat image_format;
	YCbCrFormat ycbcr_format;
	GLuint pbos[3], texture_num[3];
	bool needs_update, needs_pbo_recreate, finalized;

	int needs_mipmaps;

	unsigned width, height, widths[3], heights[3];
	const unsigned char *pixel_data[3];
	unsigned pitch[3];
};

#endif // !defined(_YCBCR_INPUT_H)
