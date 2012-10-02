#define GL_GLEXT_PROTOTYPES 1

#include <math.h>
#include <GL/gl.h>
#include <GL/glext.h>

#include "blur_effect.h"
#include "util.h"

BlurEffect::BlurEffect()
	: radius(3.0f)
{
	register_float("radius", (float *)&radius);
}

std::string BlurEffect::output_fragment_shader()
{
	return read_file("blur_effect.frag");
}

void BlurEffect::set_uniforms(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num)
{
	Effect::set_uniforms(glsl_program_num, prefix, sampler_num);

	//glActiveTexture(GL_TEXTURE0);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 3);
	//check_error();

	set_uniform_float(glsl_program_num, prefix, "pixel_offset", 1.0f / 1280.0f);  // FIXME

	// Simple Gaussian weights for now.
	float weight[15], total = 0.0f;
	for (unsigned i = 0; i < 15; ++i) {
		float z = (i - 7.0f) / radius;
		weight[i] = exp(-(z*z));
		total += weight[i];
	}
	for (unsigned i = 0; i < 15; ++i) {
		weight[i] /= total;
		printf("%f\n", weight[i]);
	}
	printf("\n");
	set_uniform_float_array(glsl_program_num, prefix, "weight", weight, 15);
}
