#include <epoxy/gl.h>
#include <Eigen/Core>
#include <stddef.h>
#include <string>
#include "util.h"

using namespace std;

namespace movit {

GLint get_uniform_location(GLuint glsl_program_num, const string &prefix, const string &key)
{
	string name = prefix + "_" + key;
	return glGetUniformLocation(glsl_program_num, name.c_str());
}

void set_uniform_int(GLuint glsl_program_num, const string &prefix, const string &key, int value)
{
	GLint location = get_uniform_location(glsl_program_num, prefix, key);
	if (location == -1) {
		return;
	}
	check_error();
	glUniform1i(location, value);
	check_error();
}

void set_uniform_float(GLuint glsl_program_num, const string &prefix, const string &key, float value)
{
	GLint location = get_uniform_location(glsl_program_num, prefix, key);
	if (location == -1) {
		return;
	}
	check_error();
	glUniform1f(location, value);
	check_error();
}

void set_uniform_vec2(GLuint glsl_program_num, const string &prefix, const string &key, const float *values)
{
	GLint location = get_uniform_location(glsl_program_num, prefix, key);
	if (location == -1) {
		return;
	}
	check_error();
	glUniform2fv(location, 1, values);
	check_error();
}

void set_uniform_vec3(GLuint glsl_program_num, const string &prefix, const string &key, const float *values)
{
	GLint location = get_uniform_location(glsl_program_num, prefix, key);
	if (location == -1) {
		return;
	}
	check_error();
	glUniform3fv(location, 1, values);
	check_error();
}

void set_uniform_vec4(GLuint glsl_program_num, const string &prefix, const string &key, const float *values)
{
	GLint location = get_uniform_location(glsl_program_num, prefix, key);
	if (location == -1) {
		return;
	}
	check_error();
	glUniform4fv(location, 1, values);
	check_error();
}

void set_uniform_vec2_array(GLuint glsl_program_num, const string &prefix, const string &key, const float *values, size_t num_values)
{
	GLint location = get_uniform_location(glsl_program_num, prefix, key);
	if (location == -1) {
		return;
	}
	check_error();
	glUniform2fv(location, num_values, values);
	check_error();
}

void set_uniform_vec4_array(GLuint glsl_program_num, const string &prefix, const string &key, const float *values, size_t num_values)
{
	GLint location = get_uniform_location(glsl_program_num, prefix, key);
	if (location == -1) {
		return;
	}
	check_error();
	glUniform4fv(location, num_values, values);
	check_error();
}

void set_uniform_mat3(GLuint glsl_program_num, const string &prefix, const string &key, const Eigen::Matrix3d& matrix)
{
	GLint location = get_uniform_location(glsl_program_num, prefix, key);
	if (location == -1) {
		return;
	}
	check_error();

	// Convert to float (GLSL has no double matrices).
	float matrixf[9];
	for (unsigned y = 0; y < 3; ++y) {
		for (unsigned x = 0; x < 3; ++x) {
			matrixf[y + x * 3] = matrix(y, x);
		}
	}

	glUniformMatrix3fv(location, 1, GL_FALSE, matrixf);
	check_error();
}

}  // namespace movit
