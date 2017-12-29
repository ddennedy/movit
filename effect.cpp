#include <epoxy/gl.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <utility>

#include "effect.h"
#include "effect_util.h"

using namespace Eigen;
using namespace std;

namespace movit {

bool Effect::set_int(const string &key, int value)
{
	if (params_int.count(key) == 0) {
		return false;
	}
	*params_int[key] = value;
	return true;
}

bool Effect::set_ivec2(const string &key, const int *values)
{
	if (params_ivec2.count(key) == 0) {
		return false;
	}
	memcpy(params_ivec2[key], values, sizeof(int) * 2);
	return true;
}

bool Effect::set_float(const string &key, float value)
{
	if (params_float.count(key) == 0) {
		return false;
	}
	*params_float[key] = value;
	return true;
}

bool Effect::set_vec2(const string &key, const float *values)
{
	if (params_vec2.count(key) == 0) {
		return false;
	}
	memcpy(params_vec2[key], values, sizeof(float) * 2);
	return true;
}

bool Effect::set_vec3(const string &key, const float *values)
{
	if (params_vec3.count(key) == 0) {
		return false;
	}
	memcpy(params_vec3[key], values, sizeof(float) * 3);
	return true;
}

bool Effect::set_vec4(const string &key, const float *values)
{
	if (params_vec4.count(key) == 0) {
		return false;
	}
	memcpy(params_vec4[key], values, sizeof(float) * 4);
	return true;
}

void Effect::register_int(const string &key, int *value)
{
	assert(params_int.count(key) == 0);
	params_int[key] = value;
	register_uniform_int(key, value);
}

void Effect::register_ivec2(const string &key, int *values)
{
	assert(params_ivec2.count(key) == 0);
	params_ivec2[key] = values;
	register_uniform_ivec2(key, values);
}

void Effect::register_float(const string &key, float *value)
{
	assert(params_float.count(key) == 0);
	params_float[key] = value;
	register_uniform_float(key, value);
}

void Effect::register_vec2(const string &key, float *values)
{
	assert(params_vec2.count(key) == 0);
	params_vec2[key] = values;
	register_uniform_vec2(key, values);
}

void Effect::register_vec3(const string &key, float *values)
{
	assert(params_vec3.count(key) == 0);
	params_vec3[key] = values;
	register_uniform_vec3(key, values);
}

void Effect::register_vec4(const string &key, float *values)
{
	assert(params_vec4.count(key) == 0);
	params_vec4[key] = values;
	register_uniform_vec4(key, values);
}

void Effect::set_gl_state(GLuint glsl_program_num, const string& prefix, unsigned *sampler_num) {}

void Effect::clear_gl_state() {}

void Effect::register_uniform_sampler2d(const std::string &key, const GLint *value)
{
	Uniform<int> uniform;
	uniform.name = key;
	uniform.value = value;
	uniform.num_values = 1;
	uniform.location = -1;
	uniforms_sampler2d.push_back(uniform);
}

void Effect::register_uniform_bool(const std::string &key, const bool *value)
{
	Uniform<bool> uniform;
	uniform.name = key;
	uniform.value = value;
	uniform.num_values = 1;
	uniform.location = -1;
	uniforms_bool.push_back(uniform);
}

void Effect::register_uniform_int(const std::string &key, const int *value)
{
	Uniform<int> uniform;
	uniform.name = key;
	uniform.value = value;
	uniform.num_values = 1;
	uniform.location = -1;
	uniforms_int.push_back(uniform);
}

void Effect::register_uniform_ivec2(const std::string &key, const int *values)
{
	Uniform<int> uniform;
	uniform.name = key;
	uniform.value = values;
	uniform.num_values = 1;
	uniform.location = -1;
	uniforms_ivec2.push_back(uniform);
}

void Effect::register_uniform_float(const std::string &key, const float *value)
{
	Uniform<float> uniform;
	uniform.name = key;
	uniform.value = value;
	uniform.num_values = 1;
	uniform.location = -1;
	uniforms_float.push_back(uniform);
}

void Effect::register_uniform_vec2(const std::string &key, const float *values)
{
	Uniform<float> uniform;
	uniform.name = key;
	uniform.value = values;
	uniform.num_values = 1;
	uniform.location = -1;
	uniforms_vec2.push_back(uniform);
}

void Effect::register_uniform_vec3(const std::string &key, const float *values)
{
	Uniform<float> uniform;
	uniform.name = key;
	uniform.value = values;
	uniform.num_values = 1;
	uniform.location = -1;
	uniforms_vec3.push_back(uniform);
}

void Effect::register_uniform_vec4(const std::string &key, const float *values)
{
	Uniform<float> uniform;
	uniform.name = key;
	uniform.value = values;
	uniform.num_values = 1;
	uniform.location = -1;
	uniforms_vec4.push_back(uniform);
}

void Effect::register_uniform_float_array(const std::string &key, const float *values, size_t num_values)
{
	Uniform<float> uniform;
	uniform.name = key;
	uniform.value = values;
	uniform.num_values = num_values;
	uniform.location = -1;
	uniforms_float_array.push_back(uniform);
}

void Effect::register_uniform_vec2_array(const std::string &key, const float *values, size_t num_values)
{
	Uniform<float> uniform;
	uniform.name = key;
	uniform.value = values;
	uniform.num_values = num_values;
	uniform.location = -1;
	uniforms_vec2_array.push_back(uniform);
}

void Effect::register_uniform_vec3_array(const std::string &key, const float *values, size_t num_values)
{
	Uniform<float> uniform;
	uniform.name = key;
	uniform.value = values;
	uniform.num_values = num_values;
	uniform.location = -1;
	uniforms_vec3_array.push_back(uniform);
}

void Effect::register_uniform_vec4_array(const std::string &key, const float *values, size_t num_values)
{
	Uniform<float> uniform;
	uniform.name = key;
	uniform.value = values;
	uniform.num_values = num_values;
	uniform.location = -1;
	uniforms_vec4_array.push_back(uniform);
}

void Effect::register_uniform_mat3(const std::string &key, const Matrix3d *matrix)
{
	Uniform<Matrix3d> uniform;
	uniform.name = key;
	uniform.value = matrix;
	uniform.num_values = 1;
	uniform.location = -1;
	uniforms_mat3.push_back(uniform);
}

}  // namespace movit
