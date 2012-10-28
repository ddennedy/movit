#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <math.h>
#include "util.h"
#include "opengl.h"
#include "init.h"

void hsv2rgb(float h, float s, float v, float *r, float *g, float *b)
{
	if (h < 0.0f) {
		h += 2.0f * M_PI;
	}
	float c = v * s;
	float hp = (h * 180.0 / M_PI) / 60.0;
	float x = c * (1 - fabs(fmod(hp, 2.0f) - 1.0f));

	if (hp >= 0 && hp < 1) {
		*r = c;
		*g = x;
		*b = 0.0f;
	} else if (hp >= 1 && hp < 2) {
		*r = x;
		*g = c;
		*b = 0.0f;
	} else if (hp >= 2 && hp < 3) {
		*r = 0.0f;
		*g = c;
		*b = x;
	} else if (hp >= 3 && hp < 4) {
		*r = 0.0f;
		*g = x;
		*b = c;
	} else if (hp >= 4 && hp < 5) {
		*r = x;
		*g = 0.0f;
		*b = c;
	} else {
		*r = c;
		*g = 0.0f;
		*b = x;
	}

	float m = v - c;
	*r += m;
	*g += m;
	*b += m;
}

void hsv2rgb_normalized(float h, float s, float v, float *r, float *g, float *b)
{
	float ref_r, ref_g, ref_b;
	hsv2rgb(h, s, v, r, g, b);
	hsv2rgb(h, 0.0f, v, &ref_r, &ref_g, &ref_b);
	float lum = 0.2126 * *r + 0.7152 * *g + 0.0722 * *b;
	float ref_lum = 0.2126 * ref_r + 0.7152 * ref_g + 0.0722 * ref_b;
	if (lum > 1e-3) {
		float fac = ref_lum / lum;
		*r *= fac;
		*g *= fac;
		*b *= fac;
	}
}

std::string read_file(const std::string &filename)
{
	static char buf[131072];
	FILE *fp = fopen(filename.c_str(), "r");
	if (fp == NULL) {
		perror(filename.c_str());
		exit(1);
	}

	int len = fread(buf, 1, sizeof(buf), fp);
	fclose(fp);

	return std::string(buf, len);
}

GLuint compile_shader(const std::string &shader_src, GLenum type)
{
	GLuint obj = glCreateShader(type);
	const GLchar* source[] = { shader_src.data() };
	const GLint length[] = { (GLint)shader_src.size() };
	glShaderSource(obj, 1, source, length);
	glCompileShader(obj);

	GLchar info_log[4096];
	GLsizei log_length = sizeof(info_log) - 1;
	glGetShaderInfoLog(obj, log_length, &log_length, info_log);
	info_log[log_length] = 0; 
	if (strlen(info_log) > 0) {
		printf("shader compile log: %s\n", info_log);
	}

	GLint status;
	glGetShaderiv(obj, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		exit(1);
	}

	return obj;
}

void print_3x3_matrix(const Eigen::Matrix3d& m)
{
	printf("%6.4f %6.4f %6.4f\n", m(0,0), m(0,1), m(0,2));
	printf("%6.4f %6.4f %6.4f\n", m(1,0), m(1,1), m(1,2));
	printf("%6.4f %6.4f %6.4f\n", m(2,0), m(2,1), m(2,2));
	printf("\n");
}

std::string output_glsl_mat3(const std::string &name, const Eigen::Matrix3d &m)
{
	char buf[1024];
	sprintf(buf,
		"const mat3 %s = mat3(\n"
		"    %.8f, %.8f, %.8f,\n"
		"    %.8f, %.8f, %.8f,\n"
		"    %.8f, %.8f, %.8f);\n\n",
		name.c_str(),
		m(0,0), m(1,0), m(2,0),
		m(0,1), m(1,1), m(2,1),
		m(0,2), m(1,2), m(2,2));
	return buf;
}

void combine_two_samples(float w1, float w2, float *offset, float *total_weight, float *sum_sq_error)
{
	assert(movit_initialized);
	assert(w1 * w2 >= 0.0f);  // Should not have differing signs.
	float z;  // Just a shorter name for offset.
	if (fabs(w1 + w2) < 1e-6) {
		z = 0.5f;
	} else {
		z = w2 / (w1 + w2);
	}

	// Round to the minimum number of bits we have measured earlier.
	// The card will do this for us anyway, but if we know what the real z
	// is, we can pick a better total_weight below.
	z = lrintf(z / movit_texel_subpixel_precision) * movit_texel_subpixel_precision;
	
	// Choose total weight w so that we minimize total squared error
	// for the effective weights:
	//
	//   e = (w(1-z) - a)² + (wz - b)²
	//
	// Differentiating by w and setting equal to zero:
	//
	//   2(w(1-z) - a)(1-z) + 2(wz - b)z = 0
	//   w(1-z)² - a(1-z) + wz² - bz = 0
	//   w((1-z)² + z²) = a(1-z) + bz
	//   w = (a(1-z) + bz) / ((1-z)² + z²)
	//
	// If z had infinite precision, this would simply reduce to w = w1 + w2.
	*total_weight = (w1 * (1 - z) + w2 * z) / (z * z + (1 - z) * (1 - z));
	*offset = z;

	if (sum_sq_error != NULL) {
		float err1 = *total_weight * (1 - z) - w1;
		float err2 = *total_weight * z - w2;
		*sum_sq_error = err1 * err1 + err2 * err2;
	}

	assert(*offset >= 0.0f);
	assert(*offset <= 1.0f);
}
