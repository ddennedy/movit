#include <epoxy/gl.h>
#include <math.h>

#include <string>
#include <vector>

#include "resource_pool.h"
#include "widgets.h"
#include "util.h"

#define HSV_WHEEL_SIZE 128

using namespace std;

namespace movit {

GLuint hsv_wheel_texnum = 0;
GLuint textured_program_num = 0, colored_program_num = 0, hsv_vao = 0;
ResourcePool resource_pool;

void draw_black_point(float x, float y, float point_size)
{
	glUseProgram(colored_program_num);
	check_error();

	float vertices[] = { x, y };
	float colors[] = { 0.0f, 0.0f, 0.0f };

	glPointSize(point_size);
	check_error();
	GLuint position_vbo = fill_vertex_attribute(colored_program_num, "position", 2, GL_FLOAT, sizeof(vertices), vertices);
	GLuint color_vbo = fill_vertex_attribute(colored_program_num, "color", 3, GL_FLOAT, sizeof(colors), colors);
	check_error();
	glDrawArrays(GL_POINTS, 0, 1);
	check_error();
	cleanup_vertex_attribute(colored_program_num, "position", position_vbo);
	cleanup_vertex_attribute(colored_program_num, "color", color_vbo);
}

void draw_hsv_wheel(float y, float rad, float theta, float value)
{
	glUseProgram(textured_program_num);
	check_error();
	glActiveTexture(GL_TEXTURE0);
	check_error();
	glBindTexture(GL_TEXTURE_2D, hsv_wheel_texnum);
	check_error();
	glUniform1i(glGetUniformLocation(textured_program_num, "tex"), 0);  // Bind the 2D sampler.
	check_error();
	glEnable(GL_BLEND);
	check_error();
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	check_error();

	GLuint vao;
	glGenVertexArrays(1, &vao);
	check_error();
	glBindVertexArray(vao);
	check_error();

	// wheel
	float wheel_vertices[] = {
		0.0f, y,
		0.0f, y + 0.2f,
		0.2f * 9.0f / 16.0f, y,
		0.2f * 9.0f / 16.0f, y + 0.2f,
	};
	float wheel_texcoords[] = {
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 1.0f,
		1.0f, 0.0f,
	};
	GLuint position_vbo = fill_vertex_attribute(textured_program_num, "position", 2, GL_FLOAT, sizeof(wheel_vertices), wheel_vertices);
	GLuint texcoord_vbo = fill_vertex_attribute(textured_program_num, "texcoord", 2, GL_FLOAT, sizeof(wheel_texcoords), wheel_texcoords);
	check_error();

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	check_error();

	cleanup_vertex_attribute(textured_program_num, "position", position_vbo);
	cleanup_vertex_attribute(textured_program_num, "texcoord", texcoord_vbo);

	// wheel selector
	draw_black_point(
	    (0.1f + rad * cos(theta) * 0.1f) * 9.0f / 16.0f,
	    y + 0.1f - rad * sin(theta) * 0.1f,
	    5.0f);

	// value slider
	glUseProgram(colored_program_num);
	float value_vertices[] = {
		0.22f * 9.0f / 16.0f, y,
		0.22f * 9.0f / 16.0f, y + 0.2f,
		0.24f * 9.0f / 16.0f, y,
		0.24f * 9.0f / 16.0f, y + 0.2f,
	};
	float value_colors[] = {
		0.0f, 0.0f, 0.0f,
		1.0f, 1.0f, 1.0f,
		0.0f, 0.0f, 0.0f,
		1.0f, 1.0f, 1.0f,
	};
	position_vbo = fill_vertex_attribute(colored_program_num, "position", 2, GL_FLOAT, sizeof(value_vertices), value_vertices);
	GLuint color_vbo = fill_vertex_attribute(colored_program_num, "color", 3, GL_FLOAT, sizeof(value_colors), value_colors);
	check_error();
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	check_error();
	cleanup_vertex_attribute(colored_program_num, "position", position_vbo);
	cleanup_vertex_attribute(colored_program_num, "color", color_vbo);

	// value selector
	draw_black_point(0.23f * 9.0f / 16.0f, y + value * 0.2f, 5.0f);

	glDeleteVertexArrays(1, &vao);
	check_error();
	glUseProgram(0);
	check_error();
}

void draw_saturation_bar(float y, float saturation)
{
	GLuint vao;
	glGenVertexArrays(1, &vao);
	check_error();
	glBindVertexArray(vao);
	check_error();

	// value slider
	glUseProgram(colored_program_num);
	float value_vertices[] = {
		0.0f * 9.0f / 16.0f, y + 0.02f,
		0.2f * 9.0f / 16.0f, y + 0.02f,
		0.0f * 9.0f / 16.0f, y,
		0.2f * 9.0f / 16.0f, y,
	};
	float value_colors[] = {
		0.0f, 0.0f, 0.0f,
		1.0f, 1.0f, 1.0f,
		0.0f, 0.0f, 0.0f,
		1.0f, 1.0f, 1.0f,
	};
	GLuint position_vbo = fill_vertex_attribute(colored_program_num, "position", 2, GL_FLOAT, sizeof(value_vertices), value_vertices);
	GLuint color_vbo = fill_vertex_attribute(colored_program_num, "color", 3, GL_FLOAT, sizeof(value_colors), value_colors);
	check_error();
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	check_error();
	cleanup_vertex_attribute(colored_program_num, "position", position_vbo);
	cleanup_vertex_attribute(colored_program_num, "color", color_vbo);

	// value selector
	draw_black_point(0.2f * saturation * 9.0f / 16.0f, y + 0.01f, 5.0f);

	glDeleteVertexArrays(1, &vao);
	check_error();
	glUseProgram(0);
	check_error();
}

void make_hsv_wheel_texture()
{
	glGenTextures(1, &hsv_wheel_texnum);

	static unsigned char hsv_pix[HSV_WHEEL_SIZE * HSV_WHEEL_SIZE * 4];
	for (int y = 0; y < HSV_WHEEL_SIZE; ++y) {
		for (int x = 0; x < HSV_WHEEL_SIZE; ++x) {
			float yf = 2.0f * y / (float)(HSV_WHEEL_SIZE) - 1.0f;
			float xf = 2.0f * x / (float)(HSV_WHEEL_SIZE) - 1.0f;
			float rad = hypot(xf, yf);
			float theta = atan2(yf, xf);

			float r, g, b;
			hsv2rgb(theta, rad, 1.0f, &r, &g, &b);
			hsv_pix[(y * HSV_WHEEL_SIZE + x) * 4 + 0] = lrintf(r * 255.0f);
			hsv_pix[(y * HSV_WHEEL_SIZE + x) * 4 + 1] = lrintf(g * 255.0f);
			hsv_pix[(y * HSV_WHEEL_SIZE + x) * 4 + 2] = lrintf(b * 255.0f);

			if (rad > 1.0f) {
				hsv_pix[(y * HSV_WHEEL_SIZE + x) * 4 + 3] = 0;
			} else {
				hsv_pix[(y * HSV_WHEEL_SIZE + x) * 4 + 3] = 255;
			}
		}
	}

	glBindTexture(GL_TEXTURE_2D, hsv_wheel_texnum);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	check_error();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, HSV_WHEEL_SIZE, HSV_WHEEL_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, hsv_pix);
	check_error();
}

void init_hsv_resources()
{
	vector<string> frag_shader_outputs;
	textured_program_num = resource_pool.compile_glsl_program(
		read_version_dependent_file("vs", "vert"),
		read_version_dependent_file("texture1d", "frag"),
		frag_shader_outputs);
	colored_program_num = resource_pool.compile_glsl_program(
		read_version_dependent_file("vs-color", "vert"),
		read_version_dependent_file("color", "frag"),
		frag_shader_outputs);
	make_hsv_wheel_texture();
}

void cleanup_hsv_resources()
{
	resource_pool.release_glsl_program(textured_program_num);
	resource_pool.release_glsl_program(colored_program_num);
}

void read_colorwheel(float xf, float yf, float *rad, float *theta, float *value)
{
	if (xf < 0.2f && yf < 0.2f) {
		float xp = 2.0f * xf / 0.2f - 1.0f;
		float yp = -(2.0f * yf / 0.2f - 1.0f);
		*rad = hypot(xp, yp);
		*theta = atan2(yp, xp);
		if (*rad > 1.0) {
			*rad = 1.0;
		}
	} else if (xf >= 0.22f && xf <= 0.24f) {
		*value = yf / 0.2f;
	}
}


}  // namespace movit
