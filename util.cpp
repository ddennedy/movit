#define GL_GLEXT_PROTOTYPES 1

#include <stdio.h>
#include <assert.h>

#include <GL/gl.h>
#include <GL/glext.h>

#include <math.h>
#include "util.h"

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

GLhandleARB compile_shader(const std::string &shader_src, GLenum type)
{
	GLhandleARB obj = glCreateShaderObjectARB(type);
	const GLchar* source[] = { shader_src.data() };
	const GLint length[] = { shader_src.size() };
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

