#include <string.h>
#include <assert.h>
#include <epoxy/gl.h>

#include "effect_util.h"
#include "flat_input.h"
#include "resource_pool.h"
#include "util.h"

using namespace std;

namespace movit {

FlatInput::FlatInput(ImageFormat image_format, MovitPixelFormat pixel_format_in, GLenum type, unsigned width, unsigned height)
	: image_format(image_format),
	  type(type),
	  pbo(0),
	  texture_num(0),
	  output_linear_gamma(false),
	  needs_mipmaps(false),
	  width(width),
	  height(height),
	  pitch(width),
	  owns_texture(false),
	  pixel_data(nullptr),
	  fixup_swap_rb(false),
	  fixup_red_to_grayscale(false)
{
	assert(type == GL_FLOAT || type == GL_HALF_FLOAT || type == GL_UNSIGNED_SHORT || type == GL_UNSIGNED_BYTE);
	register_int("output_linear_gamma", &output_linear_gamma);
	register_int("needs_mipmaps", &needs_mipmaps);
	register_uniform_sampler2d("tex", &uniform_tex);

	// Some types are not supported in all GL versions (e.g. GLES),
	// and will corrected into the right format in the shader.
	switch (pixel_format_in) {
	case FORMAT_BGRA_PREMULTIPLIED_ALPHA:
		pixel_format = FORMAT_RGBA_PREMULTIPLIED_ALPHA;
		fixup_swap_rb = true;
		break;
	case FORMAT_BGRA_POSTMULTIPLIED_ALPHA:
		pixel_format = FORMAT_RGBA_POSTMULTIPLIED_ALPHA;
		fixup_swap_rb = true;
		break;
	case FORMAT_BGR:
		pixel_format = FORMAT_RGB;
		fixup_swap_rb = true;
		break;
	case FORMAT_GRAYSCALE:
		pixel_format = FORMAT_R;
		fixup_red_to_grayscale = true;
		break;
	default:
		pixel_format = pixel_format_in;
		break;
	}
}

FlatInput::~FlatInput()
{
	possibly_release_texture();
}

void FlatInput::set_gl_state(GLuint glsl_program_num, const string& prefix, unsigned *sampler_num)
{
	glActiveTexture(GL_TEXTURE0 + *sampler_num);
	check_error();

	if (texture_num == 0 && (pbo != 0 || pixel_data != nullptr)) {
		// Translate the input format to OpenGL's enums.
		GLint internal_format;
		GLenum format;
		if (type == GL_FLOAT) {
			if (pixel_format == FORMAT_R) {
				internal_format = GL_R32F;
			} else if (pixel_format == FORMAT_RG) {
				internal_format = GL_RG32F;
			} else if (pixel_format == FORMAT_RGB) {
				internal_format = GL_RGB32F;
			} else {
				internal_format = GL_RGBA32F;
			}
		} else if (type == GL_HALF_FLOAT) {
			if (pixel_format == FORMAT_R) {
				internal_format = GL_R16F;
			} else if (pixel_format == FORMAT_RG) {
				internal_format = GL_RG16F;
			} else if (pixel_format == FORMAT_RGB) {
				internal_format = GL_RGB16F;
			} else {
				internal_format = GL_RGBA16F;
			}
		} else if (type == GL_UNSIGNED_SHORT) {
			if (pixel_format == FORMAT_R) {
				internal_format = GL_R16;
			} else if (pixel_format == FORMAT_RG) {
				internal_format = GL_RG16;
			} else if (pixel_format == FORMAT_RGB) {
				internal_format = GL_RGB16;
			} else {
				internal_format = GL_RGBA16;
			}
		} else if (output_linear_gamma) {
			assert(type == GL_UNSIGNED_BYTE);
			if (pixel_format == FORMAT_RGB) {
				internal_format = GL_SRGB8;
			} else if (pixel_format == FORMAT_RGBA_POSTMULTIPLIED_ALPHA) {
				internal_format = GL_SRGB8_ALPHA8;
			} else {
				assert(false);
			}
		} else {
			assert(type == GL_UNSIGNED_BYTE);
			if (pixel_format == FORMAT_R) {
				internal_format = GL_R8;
			} else if (pixel_format == FORMAT_RG) {
				internal_format = GL_RG8;
			} else if (pixel_format == FORMAT_RGB) {
				internal_format = GL_RGB8;
			} else {
				internal_format = GL_RGBA8;
			}
		}
		if (pixel_format == FORMAT_RGB) {
			format = GL_RGB;
		} else if (pixel_format == FORMAT_RGBA_PREMULTIPLIED_ALPHA ||
			   pixel_format == FORMAT_RGBA_POSTMULTIPLIED_ALPHA) {
			format = GL_RGBA;
		} else if (pixel_format == FORMAT_RG) {
			format = GL_RG;
		} else if (pixel_format == FORMAT_R) {
			format = GL_RED;
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
		owns_texture = true;
	} else {
		glBindTexture(GL_TEXTURE_2D, texture_num);
		check_error();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, needs_mipmaps ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR);
		check_error();
	}

	// Bind it to a sampler.
	uniform_tex = *sampler_num;
	++*sampler_num;
}

string FlatInput::output_fragment_shader()
{
	char buf[256];
	sprintf(buf, "#define FIXUP_SWAP_RB %d\n#define FIXUP_RED_TO_GRAYSCALE %d\n",
		fixup_swap_rb, fixup_red_to_grayscale);
	return buf + read_file("flat_input.frag");
}

void FlatInput::invalidate_pixel_data()
{
	possibly_release_texture();
}

void FlatInput::possibly_release_texture()
{
	if (texture_num != 0 && owns_texture) {
		resource_pool->release_2d_texture(texture_num);
		texture_num = 0;
		owns_texture = false;
	}
}

}  // namespace movit
