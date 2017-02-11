#include <epoxy/gl.h>
#include <assert.h>
#include <stdio.h>
#include <algorithm>
#include <Eigen/Core>
#include <Eigen/LU>

#include "ycbcr_conversion_effect.h"
#include "effect_util.h"
#include "util.h"
#include "ycbcr.h"

using namespace std;
using namespace Eigen;

namespace movit {

YCbCrConversionEffect::YCbCrConversionEffect(const YCbCrFormat &ycbcr_format)
	: ycbcr_format(ycbcr_format)
{
	register_uniform_mat3("ycbcr_matrix", &uniform_ycbcr_matrix);
	register_uniform_vec3("offset", uniform_offset);
	register_uniform_bool("clamp_range", &uniform_clamp_range);

	// Only used when clamp_range is true.
	register_uniform_vec3("ycbcr_min", uniform_ycbcr_min);
	register_uniform_vec3("ycbcr_max", uniform_ycbcr_max);
}

string YCbCrConversionEffect::output_fragment_shader()
{
	return read_file("ycbcr_conversion_effect.frag");
}

void YCbCrConversionEffect::set_gl_state(GLuint glsl_program_num, const string &prefix, unsigned *sampler_num)
{
	Effect::set_gl_state(glsl_program_num, prefix, sampler_num);

	Matrix3d ycbcr_to_rgb;
	compute_ycbcr_matrix(ycbcr_format, uniform_offset, &ycbcr_to_rgb);

	uniform_ycbcr_matrix = ycbcr_to_rgb.inverse();

	if (ycbcr_format.full_range) {
		// The card will clamp for us later.
		uniform_clamp_range = false;
	} else {
		uniform_clamp_range = true;

		// These limits come from BT.601 page 8, or BT.701, page 5.
		// TODO: Use num_levels. Currently we support 8-bit levels only.
		uniform_ycbcr_min[0] = 16.0 / 255.0;
		uniform_ycbcr_min[1] = 16.0 / 255.0;
		uniform_ycbcr_min[2] = 16.0 / 255.0;
		uniform_ycbcr_max[0] = 235.0 / 255.0;
		uniform_ycbcr_max[1] = 240.0 / 255.0;
		uniform_ycbcr_max[2] = 240.0 / 255.0;
	}
}

}  // namespace movit
