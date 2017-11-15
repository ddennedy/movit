#include <epoxy/gl.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "effect_util.h"
#include "resource_pool.h"
#include "util.h"
#include "ycbcr.h"
#include "ycbcr_422interleaved_input.h"

using namespace Eigen;
using namespace std;

namespace movit {

YCbCr422InterleavedInput::YCbCr422InterleavedInput(const ImageFormat &image_format,
                                                   const YCbCrFormat &ycbcr_format,
						   unsigned width, unsigned height)
	: image_format(image_format),
	  ycbcr_format(ycbcr_format),
	  width(width),
	  height(height),
	  resource_pool(nullptr)
{
	pbo = 0;
	texture_num[0] = texture_num[1] = 0;

	assert(ycbcr_format.chroma_subsampling_x == 2);
	assert(ycbcr_format.chroma_subsampling_y == 1);
	assert(width % ycbcr_format.chroma_subsampling_x == 0);

	widths[CHANNEL_LUMA] = width;
	widths[CHANNEL_CHROMA] = width / ycbcr_format.chroma_subsampling_x;
	pitches[CHANNEL_LUMA] = width;
	pitches[CHANNEL_CHROMA] = width / ycbcr_format.chroma_subsampling_x;

	pixel_data = nullptr;

	register_uniform_sampler2d("tex_y", &uniform_tex_y);
	register_uniform_sampler2d("tex_cbcr", &uniform_tex_cbcr);
}

YCbCr422InterleavedInput::~YCbCr422InterleavedInput()
{
	for (unsigned channel = 0; channel < 2; ++channel) {
		if (texture_num[channel] != 0) {
			resource_pool->release_2d_texture(texture_num[channel]);
		}
	}
}

void YCbCr422InterleavedInput::set_gl_state(GLuint glsl_program_num, const string& prefix, unsigned *sampler_num)
{
	for (unsigned channel = 0; channel < 2; ++channel) {
		glActiveTexture(GL_TEXTURE0 + *sampler_num + channel);
		check_error();

		if (texture_num[channel] == 0) {
			// (Re-)upload the texture.
			GLuint format, internal_format;
			if (channel == CHANNEL_LUMA) {
				format = GL_RG;
				internal_format = GL_RG8;
			} else {	
				assert(channel == CHANNEL_CHROMA);
				format = GL_RGBA;
				internal_format = GL_RGBA8;
			}

			texture_num[channel] = resource_pool->create_2d_texture(internal_format, widths[channel], height);
			glBindTexture(GL_TEXTURE_2D, texture_num[channel]);
			check_error();
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			check_error();
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, pbo);
			check_error();
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			check_error();
			glPixelStorei(GL_UNPACK_ROW_LENGTH, pitches[channel]);
			check_error();
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, widths[channel], height, format, GL_UNSIGNED_BYTE, pixel_data);
			check_error();
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
			check_error();
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			check_error();
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			check_error();
		} else {
			glBindTexture(GL_TEXTURE_2D, texture_num[channel]);
			check_error();
		}
	}

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
	check_error();

	// Bind samplers.
	uniform_tex_y = *sampler_num + 0;
	uniform_tex_cbcr = *sampler_num + 1;

	*sampler_num += 2;
}

string YCbCr422InterleavedInput::output_fragment_shader()
{
	float offset[3];
	Matrix3d ycbcr_to_rgb;
	compute_ycbcr_matrix(ycbcr_format, offset, &ycbcr_to_rgb);

	string frag_shader;

	frag_shader = output_glsl_mat3("PREFIX(inv_ycbcr_matrix)", ycbcr_to_rgb);
	frag_shader += output_glsl_vec3("PREFIX(offset)", offset[0], offset[1], offset[2]);

	float cb_offset_x = compute_chroma_offset(
		ycbcr_format.cb_x_position, ycbcr_format.chroma_subsampling_x, widths[CHANNEL_CHROMA]);
	float cr_offset_x = compute_chroma_offset(
		ycbcr_format.cr_x_position, ycbcr_format.chroma_subsampling_x, widths[CHANNEL_CHROMA]);
	frag_shader += output_glsl_float("PREFIX(cb_offset_x)", cb_offset_x);
	frag_shader += output_glsl_float("PREFIX(cr_offset_x)", cr_offset_x);

	char buf[256];
	sprintf(buf, "#define CB_CR_OFFSETS_EQUAL %d\n",
		(fabs(ycbcr_format.cb_x_position - ycbcr_format.cr_x_position) < 1e-6));
	frag_shader += buf;

	frag_shader += read_file("ycbcr_422interleaved_input.frag");
	return frag_shader;
}

void YCbCr422InterleavedInput::invalidate_pixel_data()
{
	for (unsigned channel = 0; channel < 2; ++channel) {
		if (texture_num[channel] != 0) {
			resource_pool->release_2d_texture(texture_num[channel]);
			texture_num[channel] = 0;
		}
	}
}

bool YCbCr422InterleavedInput::set_int(const std::string& key, int value)
{
	if (key == "needs_mipmaps") {
		// We currently do not support this.
		return (value == 0);
	}
	return Effect::set_int(key, value);
}

}  // namespace movit
