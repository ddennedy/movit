#ifndef _FLAT_INPUT_H
#define _FLAT_INPUT_H 1

#include "input.h"
#include "init.h"

// A FlatInput is the normal, “classic” case of an input, where everything
// comes from a single 2D array with chunky pixels.
class FlatInput : public Input {
public:
	FlatInput(ImageFormat format, MovitPixelFormat pixel_format, GLenum type, unsigned width, unsigned height);
	~FlatInput();

	virtual std::string effect_type_id() const { return "FlatInput"; }

	// Create the texture itself. We cannot do this in the constructor,
	// because we don't necessarily know all the settings (sRGB texture,
	// mipmap generation) at that point.
	void finalize();

	virtual bool can_output_linear_gamma() const {
		return (movit_srgb_textures_supported &&
		        type == GL_UNSIGNED_BYTE &&
		        (image_format.gamma_curve == GAMMA_LINEAR ||
		         image_format.gamma_curve == GAMMA_sRGB));
	}

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
	void set_pixel_data(const unsigned char *pixel_data)
	{
		assert(this->type == GL_UNSIGNED_BYTE);
		this->pixel_data = pixel_data;
		invalidate_pixel_data();
	}

	void set_pixel_data(const float *pixel_data)
	{
		assert(this->type == GL_FLOAT);
		this->pixel_data = pixel_data;
		invalidate_pixel_data();
	}

	void invalidate_pixel_data()
	{
		needs_update = true;
	}

	void set_pitch(unsigned pitch) {
		assert(!finalized);
		this->pitch = pitch;
	}

private:
	ImageFormat image_format;
	MovitPixelFormat pixel_format;
	GLenum format, type;
	GLuint pbo, texture_num;
	bool needs_update, finalized;
	int output_linear_gamma, needs_mipmaps;
	unsigned width, height, pitch, bytes_per_pixel;
	const void *pixel_data;
};

#endif // !defined(_FLAT_INPUT_H)
