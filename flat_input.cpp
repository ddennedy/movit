#include <string.h>
#include <assert.h>
#include <GL/glew.h>

#include "flat_input.h"
#include "util.h"

FlatInput::FlatInput(ImageFormat image_format, MovitPixelFormat pixel_format, GLenum type, unsigned width, unsigned height)
	: image_format(image_format),
          pixel_format(pixel_format),
	  type(type),
	  pbo(0),
	  texture_num(0),
	  needs_update(false),
	  finalized(false),
	  output_linear_gamma(false),
	  needs_mipmaps(false),
	  width(width),
	  height(height),
	  pitch(width),
	  pixel_data(NULL)
{
	assert(type == GL_FLOAT || type == GL_UNSIGNED_BYTE);
	register_int("output_linear_gamma", &output_linear_gamma);
	register_int("needs_mipmaps", &needs_mipmaps);
}

FlatInput::~FlatInput()
{
	if (pbo != 0) {
		glDeleteBuffers(1, &pbo);
		check_error();
	}
	if (texture_num != 0) {
		glDeleteTextures(1, &texture_num);
		check_error();
	}
}

void FlatInput::finalize()
{
	// Translate the input format to OpenGL's enums.
	GLenum internal_format;
	if (type == GL_FLOAT) {
		internal_format = GL_RGBA16F_ARB;
	} else if (output_linear_gamma) {
		assert(type == GL_UNSIGNED_BYTE);
		internal_format = GL_SRGB8_ALPHA8;
	} else {
		assert(type == GL_UNSIGNED_BYTE);
		internal_format = GL_RGBA8;
	}
	if (pixel_format == FORMAT_RGB) {
		format = GL_RGB;
		bytes_per_pixel = 3;
	} else if (pixel_format == FORMAT_RGBA_PREMULTIPLIED_ALPHA ||
	           pixel_format == FORMAT_RGBA_POSTMULTIPLIED_ALPHA) {
		format = GL_RGBA;
		bytes_per_pixel = 4;
	} else if (pixel_format == FORMAT_BGR) {
		format = GL_BGR;
		bytes_per_pixel = 3;
	} else if (pixel_format == FORMAT_BGRA_PREMULTIPLIED_ALPHA ||
	           pixel_format == FORMAT_BGRA_POSTMULTIPLIED_ALPHA) {
		format = GL_BGRA;
		bytes_per_pixel = 4;
	} else if (pixel_format == FORMAT_GRAYSCALE) {
		format = GL_LUMINANCE;
		bytes_per_pixel = 1;
	} else {
		assert(false);
	}
	if (type == GL_FLOAT) {
		bytes_per_pixel *= sizeof(float);
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
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, needs_mipmaps ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR);
	check_error();
	glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch);
	check_error();
	// Intel/Mesa seems to have a broken glGenerateMipmap() for non-FBO textures, so do it here
	// instead of calling glGenerateMipmap().
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, needs_mipmaps ? GL_TRUE : GL_FALSE);
	check_error();
	glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, type, NULL);
	check_error();
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	check_error();

	needs_update = true;
	finalized = true;
}
	
void FlatInput::set_gl_state(GLuint glsl_program_num, const std::string& prefix, unsigned *sampler_num)
{
	glActiveTexture(GL_TEXTURE0 + *sampler_num);
	check_error();
	glBindTexture(GL_TEXTURE_2D, texture_num);
	check_error();

	if (needs_update) {
		// Copy the pixel data into the PBO.
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, pbo);
		check_error();
		void *mapped_pbo = glMapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY);
		memcpy(mapped_pbo, pixel_data, pitch * height * bytes_per_pixel);
		glUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB);
		check_error();

		// Re-upload the texture from the PBO.
		glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch);
		check_error();
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, type, BUFFER_OFFSET(0));
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

std::string FlatInput::output_fragment_shader()
{
	return read_file("flat_input.frag");
}
