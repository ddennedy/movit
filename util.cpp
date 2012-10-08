#include <stdio.h>
#include <assert.h>

#include <math.h>
#include "util.h"
#include "opengl.h"

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
	printf("shader compile log: %s\n", info_log);

	GLint status;
	glGetShaderiv(obj, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		exit(1);
	}

	return obj;
}

void multiply_3x3_matrices(const Matrix3x3 a, const Matrix3x3 b, Matrix3x3 result)
{
        result[0] = a[0] * b[0] + a[3] * b[1] + a[6] * b[2];
        result[1] = a[1] * b[0] + a[4] * b[1] + a[7] * b[2];
        result[2] = a[2] * b[0] + a[5] * b[1] + a[8] * b[2];

        result[3] = a[0] * b[3] + a[3] * b[4] + a[6] * b[5];
        result[4] = a[1] * b[3] + a[4] * b[4] + a[7] * b[5];
        result[5] = a[2] * b[3] + a[5] * b[4] + a[8] * b[5];

        result[6] = a[0] * b[6] + a[3] * b[7] + a[6] * b[8];
        result[7] = a[1] * b[6] + a[4] * b[7] + a[7] * b[8];
        result[8] = a[2] * b[6] + a[5] * b[7] + a[8] * b[8];
}

void multiply_3x3_matrix_float3(const Matrix3x3 M, float x0, float x1, float x2, float *y0, float *y1, float *y2)
{
	*y0 = M[0] * x0 + M[3] * x1 + M[6] * x2;
	*y1 = M[1] * x0 + M[4] * x1 + M[7] * x2;
	*y2 = M[2] * x0 + M[5] * x1 + M[8] * x2;
}

void invert_3x3_matrix(const Matrix3x3 m, Matrix3x3 result)
{
	double inv_det = 1.0 / (
		m[6] * m[1] * m[5] - m[6] * m[2] * m[4] -
		m[3] * m[1] * m[8] + m[3] * m[2] * m[7] +
		m[0] * m[4] * m[8] - m[0] * m[5] * m[7]);

	result[0] = inv_det * (m[4] * m[8] - m[5] * m[7]);
	result[1] = inv_det * (m[2] * m[7] - m[1] * m[8]);
	result[2] = inv_det * (m[1] * m[5] - m[2] * m[4]);

	result[3] = inv_det * (m[6] * m[5] - m[3] * m[8]);
	result[4] = inv_det * (m[0] * m[8] - m[6] * m[2]);
	result[5] = inv_det * (m[3] * m[2] - m[0] * m[5]);

	result[6] = inv_det * (m[3] * m[7] - m[6] * m[4]);
	result[7] = inv_det * (m[6] * m[1] - m[0] * m[7]);
	result[8] = inv_det * (m[0] * m[4] - m[3] * m[1]);
}

void print_3x3_matrix(const Matrix3x3 m)
{
	printf("%6.4f %6.4f %6.4f\n", m[0], m[3], m[6]);
	printf("%6.4f %6.4f %6.4f\n", m[1], m[4], m[7]);
	printf("%6.4f %6.4f %6.4f\n", m[2], m[5], m[8]);
	printf("\n");
}
