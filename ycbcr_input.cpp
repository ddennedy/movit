#include <Eigen/Core>
#include <Eigen/LU>
#include <epoxy/gl.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "effect_util.h"
#include "resource_pool.h"
#include "util.h"
#include "ycbcr.h"
#include "ycbcr_input.h"

using namespace Eigen;
using namespace std;

namespace movit {

YCbCrInput::YCbCrInput(const ImageFormat &image_format,
                       const YCbCrFormat &ycbcr_format,
                       unsigned width, unsigned height,
                       YCbCrInputSplitting ycbcr_input_splitting,
                       GLenum type)
	: image_format(image_format),
	  ycbcr_format(ycbcr_format),
	  ycbcr_input_splitting(ycbcr_input_splitting),
	  needs_mipmaps(false),
	  type(type),
	  width(width),
	  height(height),
	  resource_pool(nullptr)
{
	pbos[0] = pbos[1] = pbos[2] = 0;
	texture_num[0] = texture_num[1] = texture_num[2] = 0;

	set_width(width);
	set_height(height);

	pixel_data[0] = pixel_data[1] = pixel_data[2] = nullptr;
	owns_texture[0] = owns_texture[1] = owns_texture[2] = false;

	register_uniform_sampler2d("tex_y", &uniform_tex_y);

	if (ycbcr_input_splitting == YCBCR_INPUT_INTERLEAVED) {
		num_channels = 1;
		assert(ycbcr_format.chroma_subsampling_x == 1);
		assert(ycbcr_format.chroma_subsampling_y == 1);
	} else if (ycbcr_input_splitting == YCBCR_INPUT_SPLIT_Y_AND_CBCR) {
		num_channels = 2;
		register_uniform_sampler2d("tex_cbcr", &uniform_tex_cb);
	} else {
		assert(ycbcr_input_splitting == YCBCR_INPUT_PLANAR);
		num_channels = 3;
		register_uniform_sampler2d("tex_cb", &uniform_tex_cb);
		register_uniform_sampler2d("tex_cr", &uniform_tex_cr);
	}

	register_int("needs_mipmaps", &needs_mipmaps);
	register_uniform_mat3("inv_ycbcr_matrix", &uniform_ycbcr_matrix);
	register_uniform_vec3("offset", uniform_offset);
	register_uniform_vec2("cb_offset", (float *)&uniform_cb_offset);
	register_uniform_vec2("cr_offset", (float *)&uniform_cr_offset);
}

YCbCrInput::~YCbCrInput()
{
	for (unsigned channel = 0; channel < num_channels; ++channel) {
		possibly_release_texture(channel);
	}
}

void YCbCrInput::set_gl_state(GLuint glsl_program_num, const string& prefix, unsigned *sampler_num)
{
	compute_ycbcr_matrix(ycbcr_format, uniform_offset, &uniform_ycbcr_matrix, type);

	uniform_cb_offset.x = compute_chroma_offset(
		ycbcr_format.cb_x_position, ycbcr_format.chroma_subsampling_x, widths[1]);
	uniform_cb_offset.y = compute_chroma_offset(
		ycbcr_format.cb_y_position, ycbcr_format.chroma_subsampling_y, heights[1]);

	uniform_cr_offset.x = compute_chroma_offset(
		ycbcr_format.cr_x_position, ycbcr_format.chroma_subsampling_x, widths[2]);
	uniform_cr_offset.y = compute_chroma_offset(
		ycbcr_format.cr_y_position, ycbcr_format.chroma_subsampling_y, heights[2]);

	for (unsigned channel = 0; channel < num_channels; ++channel) {
		glActiveTexture(GL_TEXTURE0 + *sampler_num + channel);
		check_error();

		if (texture_num[channel] == 0 && (pbos[channel] != 0 || pixel_data[channel] != nullptr)) {
			GLenum format, internal_format;
			if (channel == 0 && ycbcr_input_splitting == YCBCR_INPUT_INTERLEAVED) {
				if (type == GL_UNSIGNED_INT_2_10_10_10_REV) {
					format = GL_RGBA;
					internal_format = GL_RGB10_A2;
				} else if (type == GL_UNSIGNED_SHORT) {
					format = GL_RGB;
					internal_format = GL_RGB16;
				} else {
					assert(type == GL_UNSIGNED_BYTE);
					format = GL_RGB;
					internal_format = GL_RGB8;
				}
			} else if (channel == 1 && ycbcr_input_splitting == YCBCR_INPUT_SPLIT_Y_AND_CBCR) {
				format = GL_RG;
				if (type == GL_UNSIGNED_SHORT) {
					internal_format = GL_RG16;
				} else {
					assert(type == GL_UNSIGNED_BYTE);
					internal_format = GL_RG8;
				}
			} else {
				format = GL_RED;
				if (type == GL_UNSIGNED_SHORT) {
					internal_format = GL_R16;
				} else {
					assert(type == GL_UNSIGNED_BYTE);
					internal_format = GL_R8;
				}
			}

			// (Re-)upload the texture.
			texture_num[channel] = resource_pool->create_2d_texture(internal_format, widths[channel], heights[channel]);
			glBindTexture(GL_TEXTURE_2D, texture_num[channel]);
			check_error();
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, needs_mipmaps ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR);
			check_error();
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, pbos[channel]);
			check_error();
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			check_error();
			glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch[channel]);
			check_error();
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, widths[channel], heights[channel], format, type, pixel_data[channel]);
			check_error();
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
			check_error();
			if (needs_mipmaps) {
				glGenerateMipmap(GL_TEXTURE_2D);
				check_error();
			}
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			check_error();
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			check_error();
			owns_texture[channel] = true;
		} else {
			glBindTexture(GL_TEXTURE_2D, texture_num[channel]);
			check_error();
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, needs_mipmaps ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR);
			check_error();
		}
	}

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
	check_error();

	// Bind samplers.
	uniform_tex_y = *sampler_num + 0;
	if (ycbcr_input_splitting != YCBCR_INPUT_INTERLEAVED) {
		uniform_tex_cb = *sampler_num + 1;
	}
	if (ycbcr_input_splitting == YCBCR_INPUT_PLANAR) {
		uniform_tex_cr = *sampler_num + 2;
	}

	*sampler_num += num_channels;
}

string YCbCrInput::output_fragment_shader()
{
	string frag_shader;

	if (ycbcr_input_splitting == YCBCR_INPUT_INTERLEAVED) {
		frag_shader += "#define Y_CB_CR_SAME_TEXTURE 1\n";
	} else if (ycbcr_input_splitting == YCBCR_INPUT_SPLIT_Y_AND_CBCR) {
		cb_cr_offsets_equal =
			(fabs(ycbcr_format.cb_x_position - ycbcr_format.cr_x_position) < 1e-6) &&
			(fabs(ycbcr_format.cb_y_position - ycbcr_format.cr_y_position) < 1e-6);
		char buf[256];
		snprintf(buf, sizeof(buf), "#define Y_CB_CR_SAME_TEXTURE 0\n#define CB_CR_SAME_TEXTURE 1\n#define CB_CR_OFFSETS_EQUAL %d\n",
			cb_cr_offsets_equal);
		frag_shader += buf;
	} else {
		frag_shader += "#define Y_CB_CR_SAME_TEXTURE 0\n#define CB_CR_SAME_TEXTURE 0\n";
	}

	frag_shader += read_file("ycbcr_input.frag");
	frag_shader += "#undef CB_CR_SAME_TEXTURE\n#undef Y_CB_CR_SAME_TEXTURE\n";
	return frag_shader;
}

void YCbCrInput::change_ycbcr_format(const YCbCrFormat &ycbcr_format)
{
	if (ycbcr_input_splitting == YCBCR_INPUT_SPLIT_Y_AND_CBCR && cb_cr_offsets_equal) {
		assert((fabs(ycbcr_format.cb_x_position - ycbcr_format.cr_x_position) < 1e-6) &&
		       (fabs(ycbcr_format.cb_y_position - ycbcr_format.cr_y_position) < 1e-6));
	}
	if (ycbcr_input_splitting == YCBCR_INPUT_INTERLEAVED) {
		assert(ycbcr_format.chroma_subsampling_x == 1);
		assert(ycbcr_format.chroma_subsampling_y == 1);
	}
	this->ycbcr_format = ycbcr_format;
}

void YCbCrInput::invalidate_pixel_data()
{
	for (unsigned channel = 0; channel < 3; ++channel) {
		possibly_release_texture(channel);
	}
}

bool YCbCrInput::set_int(const std::string& key, int value)
{
	if (key == "needs_mipmaps") {
		if (ycbcr_input_splitting != YCBCR_INPUT_INTERLEAVED && value != 0) {
			// We do not currently support this.
			return false;
		}
	}
	return Effect::set_int(key, value);
}

void YCbCrInput::possibly_release_texture(unsigned channel)
{
	if (texture_num[channel] != 0 && owns_texture[channel]) {
		resource_pool->release_2d_texture(texture_num[channel]);
		texture_num[channel] = 0;
		owns_texture[channel] = false;
	}
}

}  // namespace movit
