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
                       unsigned width, unsigned height)
	: image_format(image_format),
	  ycbcr_format(ycbcr_format),
	  width(width),
	  height(height),
	  resource_pool(NULL)
{
	pbos[0] = pbos[1] = pbos[2] = 0;
	texture_num[0] = texture_num[1] = texture_num[2] = 0;

	assert(width % ycbcr_format.chroma_subsampling_x == 0);
	pitch[0] = widths[0] = width;
	pitch[1] = widths[1] = width / ycbcr_format.chroma_subsampling_x;
	pitch[2] = widths[2] = width / ycbcr_format.chroma_subsampling_x;

	assert(height % ycbcr_format.chroma_subsampling_y == 0);
	heights[0] = height;
	heights[1] = height / ycbcr_format.chroma_subsampling_y;
	heights[2] = height / ycbcr_format.chroma_subsampling_y;

	pixel_data[0] = pixel_data[1] = pixel_data[2] = NULL;

	register_uniform_sampler2d("tex_y", &uniform_tex_y);
	register_uniform_sampler2d("tex_cb", &uniform_tex_cb);
	register_uniform_sampler2d("tex_cr", &uniform_tex_cr);
}

YCbCrInput::~YCbCrInput()
{
	for (unsigned channel = 0; channel < 3; ++channel) {
		if (texture_num[channel] != 0) {
			resource_pool->release_2d_texture(texture_num[channel]);
		}
	}
}

void YCbCrInput::set_gl_state(GLuint glsl_program_num, const string& prefix, unsigned *sampler_num)
{
	for (unsigned channel = 0; channel < 3; ++channel) {
		glActiveTexture(GL_TEXTURE0 + *sampler_num + channel);
		check_error();

		if (texture_num[channel] == 0) {
			// (Re-)upload the texture.
			texture_num[channel] = resource_pool->create_2d_texture(GL_R8, widths[channel], heights[channel]);
			glBindTexture(GL_TEXTURE_2D, texture_num[channel]);
			check_error();
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			check_error();
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, pbos[channel]);
			check_error();
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			check_error();
			glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch[channel]);
			check_error();
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, widths[channel], heights[channel], GL_RED, GL_UNSIGNED_BYTE, pixel_data[channel]);
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
	uniform_tex_cb = *sampler_num + 1;
	uniform_tex_cr = *sampler_num + 2;

	*sampler_num += 3;
}

string YCbCrInput::output_fragment_shader()
{
	float offset[3];
	Matrix3d ycbcr_to_rgb;
	compute_ycbcr_matrix(ycbcr_format, offset, &ycbcr_to_rgb);

	string frag_shader;

	frag_shader = output_glsl_mat3("PREFIX(inv_ycbcr_matrix)", ycbcr_to_rgb);
	frag_shader += output_glsl_vec3("PREFIX(offset)", offset[0], offset[1], offset[2]);

	float cb_offset_x = compute_chroma_offset(
		ycbcr_format.cb_x_position, ycbcr_format.chroma_subsampling_x, widths[1]);
	float cb_offset_y = compute_chroma_offset(
		ycbcr_format.cb_y_position, ycbcr_format.chroma_subsampling_y, heights[1]);
	frag_shader += output_glsl_vec2("PREFIX(cb_offset)", cb_offset_x, cb_offset_y);

	float cr_offset_x = compute_chroma_offset(
		ycbcr_format.cr_x_position, ycbcr_format.chroma_subsampling_x, widths[2]);
	float cr_offset_y = compute_chroma_offset(
		ycbcr_format.cr_y_position, ycbcr_format.chroma_subsampling_y, heights[2]);
	frag_shader += output_glsl_vec2("PREFIX(cr_offset)", cr_offset_x, cr_offset_y);

	frag_shader += read_file("ycbcr_input.frag");
	return frag_shader;
}

void YCbCrInput::invalidate_pixel_data()
{
	for (unsigned channel = 0; channel < 3; ++channel) {
		if (texture_num[channel] != 0) {
			resource_pool->release_2d_texture(texture_num[channel]);
			texture_num[channel] = 0;
		}
	}
}

bool YCbCrInput::set_int(const std::string& key, int value)
{
	if (key == "needs_mipmaps") {
		// We currently do not support this.
		return (value == 0);
	}
	return Effect::set_int(key, value);
}

}  // namespace movit
