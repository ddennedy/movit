#ifndef _INPUT_H
#define _INPUT_H 1

#include "effect.h"
#include "image_format.h"

// An input is a degenerate case of an effect; it represents the picture data
// that comes from the user. As such, it has zero “inputs” itself, and some of
// the normal operations don't really make sense for it.
class Input : public Effect {
public:
	Input(ImageFormat format, unsigned width, unsigned height);

	unsigned num_inputs() const { return 0; }

	// Create the texture itself. We cannot do this in the constructor,
	// because we don't necessarily know all the settings (sRGB texture,
	// mipmap generation) at that point.
	void finalize();

	std::string output_fragment_shader();

	// Uploads the texture if it has changed since last time.
	void set_gl_state(GLuint glsl_program_num, const std::string& prefix, unsigned *sampler_num);

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

private:
	ImageFormat image_format;
	GLenum format;
	GLuint texture_num;
	bool needs_update;
	int use_srgb_texture_format, needs_mipmaps;
	unsigned width, height, bytes_per_pixel;
	const unsigned char *pixel_data;
};

#endif // !defined(_INPUT_H)
