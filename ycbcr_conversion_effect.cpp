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
}

string YCbCrConversionEffect::output_fragment_shader()
{
	float offset[3];
	Matrix3d ycbcr_to_rgb;
	compute_ycbcr_matrix(ycbcr_format, offset, &ycbcr_to_rgb);

        string frag_shader = output_glsl_mat3("PREFIX(ycbcr_matrix)", ycbcr_to_rgb.inverse());
        frag_shader += output_glsl_vec3("PREFIX(offset)", offset[0], offset[1], offset[2]);

	if (ycbcr_format.full_range) {
		// The card will clamp for us later.
		frag_shader += "#define YCBCR_CLAMP_RANGE 0\n";
	} else {
		frag_shader += "#define YCBCR_CLAMP_RANGE 1\n";

		// These limits come from BT.601 page 8, or BT.701, page 5.
		// TODO: Use num_levels. Currently we support 8-bit levels only.
		frag_shader += output_glsl_vec3("PREFIX(ycbcr_min)", 16.0 / 255.0, 16.0 / 255.0, 16.0 / 255.0);
		frag_shader += output_glsl_vec3("PREFIX(ycbcr_max)", 235.0 / 255.0, 240.0 / 255.0, 240.0 / 255.0);
	}

	return frag_shader + read_file("ycbcr_conversion_effect.frag");
}

}  // namespace movit
