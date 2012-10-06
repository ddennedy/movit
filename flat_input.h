#ifndef _FLAT_INPUT_H
#define _FLAT_INPUT_H 1

#include "input.h"

// A FlatInput is the normal, “classic” case of an input, where everything
// comes from a single 2D array with chunky pixels.
class FlatInput : public Input {
public:
	FlatInput(ImageFormat format, unsigned width, unsigned height);

	// Create the texture itself. We cannot do this in the constructor,
	// because we don't necessarily know all the settings (sRGB texture,
	// mipmap generation) at that point.
	void finalize();

	std::string output_fragment_shader();

	// Uploads the texture if it has changed since last time.
	void set_gl_state(GLuint glsl_program_num, const std::string& prefix, unsigned *sampler_num);

	ColorSpace get_color_space() { return image_format.color_space; }
	GammaCurve get_gamma_curve() { return image_format.gamma_curve; }

	// Tells the input where to fetch the actual pixel data. Note that if you change
	// this data, you must either call set_pixel_data() again (using the same pointer
	// is fine), or invalidate_pixel_data(). Otherwise, the texture won't be re-uploaded
	// on subsequent frames.
	void set_pixel_data(const unsigned char *pixel_data)
	{
		this->pixel_data = pixel_data;
		invalidate_pixel_data();
	}

	void invalidate_pixel_data()
	{
		needs_update = true;
	}

	const unsigned char *get_pixel_data() const
	{
		return pixel_data;
	}

	void set_pitch(unsigned pitch) {
		assert(!finalized);
		this->pitch = pitch;
	}

	unsigned get_pitch() {
		return pitch;
	}

private:
	ImageFormat image_format;
	GLenum format;
	GLuint pbo, texture_num;
	bool needs_update, finalized;
	int use_srgb_texture_format, needs_mipmaps;
	unsigned width, height, pitch, bytes_per_pixel;
	const unsigned char *pixel_data;
};

#endif // !defined(_FLAT_INPUT_H)
