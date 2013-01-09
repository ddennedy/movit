#include <string.h>
#include <assert.h>
#include <GL/glew.h>

#include <Eigen/LU>

#include "ycbcr_input.h"
#include "util.h"

using namespace Eigen;

namespace {

// OpenGL has texel center in (0.5, 0.5), but different formats have
// chroma in various other places. If luma samples are X, the chroma
// sample is *, and subsampling is 3x3, the situation with chroma
// center in (0.5, 0.5) looks approximately like this:
//
//   X   X
//     *   
//   X   X
//
// If, on the other hand, chroma center is in (0.0, 0.5) (common
// for e.g. MPEG-4), the figure changes to:
//
//   X   X
//   *      
//   X   X
//
// In other words, (0.0, 0.0) means that the chroma sample is exactly
// co-sited on top of the top-left luma sample. Note, however, that
// this is _not_ 0.5 texels to the left, since the OpenGL's texel center
// is in (0.5, 0.5); it is in (0.25, 0.25). In a sense, the four luma samples
// define a square where chroma position (0.0, 0.0) is in texel position
// (0.25, 0.25) and chroma position (1.0, 1.0) is in texel position (0.75, 0.75)
// (the outer border shows the borders of the texel itself, ie. from
// (0, 0) to (1, 1)):
//
//  ---------
// |         |
// |  X---X  |
// |  | * |  |
// |  X---X  |
// |         |
//  ---------
//
// Also note that if we have no subsampling, the square will have zero
// area and the chroma position does not matter at all.
float compute_chroma_offset(float pos, unsigned subsampling_factor, unsigned resolution)
{
	float local_chroma_pos = (0.5 + pos * (subsampling_factor - 1)) / subsampling_factor;
	return (0.5 - local_chroma_pos) / resolution;
}

}  // namespace

YCbCrInput::YCbCrInput(const ImageFormat &image_format,
                       const YCbCrFormat &ycbcr_format,
                       unsigned width, unsigned height)
	: image_format(image_format),
	  ycbcr_format(ycbcr_format),
	  needs_update(false),
	  needs_pbo_recreate(false),
	  finalized(false),
	  needs_mipmaps(false),
	  width(width),
	  height(height)
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

	register_int("needs_mipmaps", &needs_mipmaps);
}

YCbCrInput::~YCbCrInput()
{
	if (pbos[0] != 0) {
		glDeleteBuffers(3, pbos);
		check_error();
	}
	if (texture_num[0] != 0) {
		glDeleteTextures(3, texture_num);
		check_error();
	}
}

void YCbCrInput::finalize()
{
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	check_error();

	// Create PBOs to hold the textures holding the input image, and then the texture itself.
	glGenBuffers(3, pbos);
	check_error();
	glGenTextures(3, texture_num);
	check_error();

	for (unsigned channel = 0; channel < 3; ++channel) {
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, pbos[channel]);
		check_error();
		glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, pitch[channel] * heights[channel], NULL, GL_STREAM_DRAW);
		check_error();
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
		check_error();
		
		glBindTexture(GL_TEXTURE_2D, texture_num[channel]);
		check_error();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		check_error();
		glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch[channel]);
		check_error();
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE8, widths[channel], heights[channel], 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
		check_error();
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		check_error();
	}

	needs_update = true;
	finalized = true;
}
	
void YCbCrInput::set_gl_state(GLuint glsl_program_num, const std::string& prefix, unsigned *sampler_num)
{
	for (unsigned channel = 0; channel < 3; ++channel) {
		glActiveTexture(GL_TEXTURE0 + *sampler_num + channel);
		check_error();
		glBindTexture(GL_TEXTURE_2D, texture_num[channel]);
		check_error();

		if (needs_update || needs_pbo_recreate) {
			// Copy the pixel data into the PBO.
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, pbos[channel]);
			check_error();

			if (needs_pbo_recreate) {
				// The pitch has changed; we need to reallocate this PBO.
				glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, pitch[channel] * heights[channel], NULL, GL_STREAM_DRAW);
				check_error();
			}

			void *mapped_pbo = glMapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY);
			memcpy(mapped_pbo, pixel_data[channel], pitch[channel] * heights[channel]);

			glUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB);
			check_error();

			// Re-upload the texture from the PBO.
			glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch[channel]);
			check_error();
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, widths[channel], heights[channel], GL_LUMINANCE, GL_UNSIGNED_BYTE, BUFFER_OFFSET(0));
			check_error();
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
			check_error();
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			check_error();
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			check_error();
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
			check_error();
		}
	}

	// Bind samplers.
	set_uniform_int(glsl_program_num, prefix, "tex_y", *sampler_num + 0);
	set_uniform_int(glsl_program_num, prefix, "tex_cb", *sampler_num + 1);
	set_uniform_int(glsl_program_num, prefix, "tex_cr", *sampler_num + 2);

	*sampler_num += 3;
	needs_update = false;
	needs_pbo_recreate = false;
}

std::string YCbCrInput::output_fragment_shader()
{
	float coeff[3], offset[3], scale[3];

	switch (ycbcr_format.luma_coefficients) {
	case YCBCR_REC_601:
		// Rec. 601, page 2.
		coeff[0] = 0.299;
		coeff[1] = 0.587;
		coeff[2] = 0.114;
		break;

	case YCBCR_REC_709:
		// Rec. 709, page 19.
		coeff[0] = 0.2126;
		coeff[1] = 0.7152;
		coeff[2] = 0.0722;
		break;
	default:
		assert(false);
	}

	if (ycbcr_format.full_range) {
		offset[0] = 0.0 / 255.0;
		offset[1] = 128.0 / 255.0;
		offset[2] = 128.0 / 255.0;

		scale[0] = 1.0;
		scale[1] = 1.0;
		scale[2] = 1.0;
	} else {
		// Rec. 601, page 4; Rec. 709, page 19.
		offset[0] = 16.0 / 255.0;
		offset[1] = 128.0 / 255.0;
		offset[2] = 128.0 / 255.0;

		scale[0] = 255.0 / 219.0;
		scale[1] = 255.0 / 224.0;
		scale[2] = 255.0 / 224.0;
	}

	// Matrix to convert RGB to YCbCr. See e.g. Rec. 601.
	Matrix3d rgb_to_ycbcr;
	rgb_to_ycbcr(0,0) = coeff[0];
	rgb_to_ycbcr(0,1) = coeff[1];
	rgb_to_ycbcr(0,2) = coeff[2];

	float cb_fac = (224.0 / 219.0) / (coeff[0] + coeff[1] + 1.0f - coeff[2]);
	rgb_to_ycbcr(1,0) = -coeff[0] * cb_fac;
	rgb_to_ycbcr(1,1) = -coeff[1] * cb_fac;
	rgb_to_ycbcr(1,2) = (1.0f - coeff[2]) * cb_fac;

	float cr_fac = (224.0 / 219.0) / (1.0f - coeff[0] + coeff[1] + coeff[2]);
	rgb_to_ycbcr(2,0) = (1.0f - coeff[0]) * cr_fac;
	rgb_to_ycbcr(2,1) = -coeff[1] * cr_fac;
	rgb_to_ycbcr(2,2) = -coeff[2] * cr_fac;

	// Inverting the matrix gives us what we need to go from YCbCr back to RGB.
	Matrix3d ycbcr_to_rgb = rgb_to_ycbcr.inverse();

	std::string frag_shader;

	frag_shader = output_glsl_mat3("PREFIX(inv_ycbcr_matrix)", ycbcr_to_rgb);

	char buf[256];
	sprintf(buf, "const vec3 PREFIX(offset) = vec3(%.8f, %.8f, %.8f);\n",
		offset[0], offset[1], offset[2]);
	frag_shader += buf;

	sprintf(buf, "const vec3 PREFIX(scale) = vec3(%.8f, %.8f, %.8f);\n",
		scale[0], scale[1], scale[2]);
	frag_shader += buf;

	float cb_offset_x = compute_chroma_offset(
		ycbcr_format.cb_x_position, ycbcr_format.chroma_subsampling_x, widths[1]);
	float cb_offset_y = compute_chroma_offset(
		ycbcr_format.cb_y_position, ycbcr_format.chroma_subsampling_y, heights[1]);
	sprintf(buf, "const vec2 PREFIX(cb_offset) = vec2(%.8f, %.8f);\n",
		cb_offset_x, cb_offset_y);
	frag_shader += buf;

	float cr_offset_x = compute_chroma_offset(
		ycbcr_format.cr_x_position, ycbcr_format.chroma_subsampling_x, widths[2]);
	float cr_offset_y = compute_chroma_offset(
		ycbcr_format.cr_y_position, ycbcr_format.chroma_subsampling_y, heights[2]);
	sprintf(buf, "const vec2 PREFIX(cr_offset) = vec2(%.8f, %.8f);\n",
		cr_offset_x, cr_offset_y);
	frag_shader += buf;

	frag_shader += read_file("ycbcr_input.frag");
	return frag_shader;
}
