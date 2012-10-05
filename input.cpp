#define GL_GLEXT_PROTOTYPES 1

#include <string.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <assert.h>

#include "input.h"
#include "util.h"

Input::Input(ImageFormat image_format, unsigned width, unsigned height)
	: image_format(image_format),
	  needs_update(false),
	  finalized(false),
	  use_srgb_texture_format(false),
	  needs_mipmaps(false),
	  width(width),
	  height(height),
	  pitch(width)
{
	register_int("use_srgb_texture_format", &use_srgb_texture_format);
	register_int("needs_mipmaps", &needs_mipmaps);
}

void Input::finalize()
{
	// Translate the input format to OpenGL's enums.
	GLenum internal_format;
	if (use_srgb_texture_format) {
		internal_format = GL_SRGB8;
	} else {
		internal_format = GL_RGBA8;
	}
	if (image_format.pixel_format == FORMAT_RGB) {
		format = GL_RGB;
		bytes_per_pixel = 3;
	} else if (image_format.pixel_format == FORMAT_RGBA) {
		format = GL_RGBA;
		bytes_per_pixel = 4;
	} else if (image_format.pixel_format == FORMAT_BGR) {
		format = GL_BGR;
		bytes_per_pixel = 3;
	} else if (image_format.pixel_format == FORMAT_BGRA) {
		format = GL_BGRA;
		bytes_per_pixel = 4;
	} else if (image_format.pixel_format == FORMAT_GRAYSCALE) {
		format = GL_LUMINANCE;
		bytes_per_pixel = 1;
	} else {
		assert(false);
	}

	// Create PBO to hold the texture holding the input image, and then the texture itself.
	glGenBuffers(1, &pbo);
	check_error();
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, pbo);
	check_error();
	glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, pitch * height * bytes_per_pixel, NULL, GL_STREAM_DRAW);
	check_error();
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
	check_error();
	
	glGenTextures(1, &texture_num);
	check_error();
	glBindTexture(GL_TEXTURE_2D, texture_num);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	check_error();
	glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch);
	check_error();
	// Intel/Mesa seems to have a broken glGenerateMipmap() for non-FBO textures, so do it here
	// instead of calling glGenerateMipmap().
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, needs_mipmaps);
	check_error();
	glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, GL_UNSIGNED_BYTE, NULL);
	check_error();
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	check_error();

	needs_update = false;
	finalized = true;
}
	
void Input::set_gl_state(GLuint glsl_program_num, const std::string& prefix, unsigned *sampler_num)
{
	if (needs_update) {
		// Copy the pixel data into the PBO.
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, pbo);
		check_error();
		void *mapped_pbo = glMapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY);
		memcpy(mapped_pbo, pixel_data, pitch * height * bytes_per_pixel);
		glUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB);
		check_error();

		// Re-upload the texture from the PBO.
		glActiveTexture(GL_TEXTURE0);
		check_error();
		glBindTexture(GL_TEXTURE_2D, texture_num);
		check_error();
		glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch);
		check_error();
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, GL_UNSIGNED_BYTE, BUFFER_OFFSET(0));
		check_error();
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		check_error();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		check_error();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		check_error();
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
		check_error();

		needs_update = false;
	}

	// Bind it to a sampler.
	set_uniform_int(glsl_program_num, prefix, "tex", *sampler_num);
	++*sampler_num;
}

std::string Input::output_fragment_shader()
{
	return read_file("input.frag");
}
