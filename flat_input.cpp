#include <string.h>
#include <assert.h>
#include <epoxy/gl.h>

#include "effect_util.h"
#include "flat_input.h"
#include "resource_pool.h"
#include "util.h"

using namespace std;

namespace movit {

FlatInput::FlatInput(ImageFormat image_format, MovitPixelFormat pixel_format, GLenum type, unsigned width, unsigned height)
	: image_format(image_format),
          pixel_format(pixel_format),
	  type(type),
	  pbo(0),
	  texture_num(0),
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
	if (texture_num != 0) {
		resource_pool->release_2d_texture(texture_num);
	}
}

void FlatInput::set_gl_state(GLuint glsl_program_num, const string& prefix, unsigned *sampler_num)
{
	glActiveTexture(GL_TEXTURE0 + *sampler_num);
	check_error();

	if (texture_num == 0) {
		// Translate the input format to OpenGL's enums.
		GLint internal_format;
		GLenum format;
		if (type == GL_FLOAT) {
			internal_format = GL_RGBA32F_ARB;
		} else if (output_linear_gamma) {
			assert(type == GL_UNSIGNED_BYTE);
			internal_format = GL_SRGB8_ALPHA8;
		} else {
			assert(type == GL_UNSIGNED_BYTE);
			internal_format = GL_RGBA8;
		}
		if (pixel_format == FORMAT_RGB) {
			format = GL_RGB;
		} else if (pixel_format == FORMAT_RGBA_PREMULTIPLIED_ALPHA ||
			   pixel_format == FORMAT_RGBA_POSTMULTIPLIED_ALPHA) {
			format = GL_RGBA;
		} else if (pixel_format == FORMAT_BGR) {
			format = GL_BGR;
		} else if (pixel_format == FORMAT_BGRA_PREMULTIPLIED_ALPHA ||
			   pixel_format == FORMAT_BGRA_POSTMULTIPLIED_ALPHA) {
			format = GL_BGRA;
		} else if (pixel_format == FORMAT_GRAYSCALE) {
			format = GL_LUMINANCE;
		} else {
			assert(false);
		}

		// (Re-)upload the texture.
		texture_num = resource_pool->create_2d_texture(internal_format, width, height);
		glBindTexture(GL_TEXTURE_2D, texture_num);
		check_error();
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, pbo);
		check_error();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, needs_mipmaps ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR);
		check_error();
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		check_error();
		glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch);
		check_error();
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, type, pixel_data);
		check_error();
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		check_error();
		if (needs_mipmaps) {
			glGenerateMipmap(GL_TEXTURE_2D);
			check_error();
		}
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		check_error();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		check_error();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		check_error();
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
		check_error();
	} else {
		glBindTexture(GL_TEXTURE_2D, texture_num);
		check_error();
	}

	// Bind it to a sampler.
	set_uniform_int(glsl_program_num, prefix, "tex", *sampler_num);
	++*sampler_num;
}

string FlatInput::output_fragment_shader()
{
	return read_file("flat_input.frag");
}

void FlatInput::invalidate_pixel_data()
{
	if (texture_num != 0) {
		resource_pool->release_2d_texture(texture_num);
		texture_num = 0;
	}
}

}  // namespace movit
