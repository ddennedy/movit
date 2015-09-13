#ifndef _MOVIT_EFFECT_UTIL_H
#define _MOVIT_EFFECT_UTIL_H 1

// Utilities that are often useful for implementing Effect instances,
// but don't need to be included from effect.h.

#include <epoxy/gl.h>
#include <assert.h>
#include <stddef.h>
#include <Eigen/Core>
#include <map>
#include <string>
#include <vector>

#include "util.h"

namespace movit {

class EffectChain;
class Node;

// Convenience functions that deal with prepending the prefix.
// Note that using EffectChain::register_uniform_*() is more efficient
// than calling these from set_gl_state().
GLint get_uniform_location(GLuint glsl_program_num, const std::string &prefix, const std::string &key);
void set_uniform_int(GLuint glsl_program_num, const std::string &prefix, const std::string &key, int value);
void set_uniform_float(GLuint glsl_program_num, const std::string &prefix, const std::string &key, float value);
void set_uniform_vec2(GLuint glsl_program_num, const std::string &prefix, const std::string &key, const float *values);
void set_uniform_vec3(GLuint glsl_program_num, const std::string &prefix, const std::string &key, const float *values);
void set_uniform_vec4(GLuint glsl_program_num, const std::string &prefix, const std::string &key, const float *values);
void set_uniform_vec2_array(GLuint glsl_program_num, const std::string &prefix, const std::string &key, const float *values, size_t num_values);
void set_uniform_vec4_array(GLuint glsl_program_num, const std::string &prefix, const std::string &key, const float *values, size_t num_values);
void set_uniform_mat3(GLuint glsl_program_num, const std::string &prefix, const std::string &key, const Eigen::Matrix3d &matrix);

}  // namespace movit

#endif // !defined(_MOVIT_EFFECT_UTIL_H)
