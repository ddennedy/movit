#ifndef _MOVIT_UTIL_H
#define _MOVIT_UTIL_H 1

// Various utilities.

#include <epoxy/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <Eigen/Core>
#include <string>
#include "defs.h"
#include "fp16.h"

#define BUFFER_OFFSET(i) ((char *)nullptr + (i))

namespace movit {

// Converts a HSV color to RGB. Assumes h in [0, 2pi> or [-pi, pi>
void hsv2rgb(float h, float s, float v, float *r, float *g, float *b);

// Converts a HSV color to RGB, but keeps luminance constant
// (ie. color luminance is as if S=0).
void hsv2rgb_normalized(float h, float s, float v, float *r, float *g, float *b);

// Read a file from disk and return its contents.
// Dies if the file does not exist.
std::string read_file(const std::string &filename);

// Reads <base>.<extension>, <base>.130.<extension> or <base>.300es.<extension> and
// returns its contents, depending on <movit_shader_level>.
std::string read_version_dependent_file(const std::string &base, const std::string &extension);

// Compile the given GLSL shader (typically a vertex or fragment shader)
// and return the object number.
GLuint compile_shader(const std::string &shader_src, GLenum type);

// Print a 3x3 matrix to standard output. Useful for debugging.
void print_3x3_matrix(const Eigen::Matrix3d &m);

// Output a GLSL 3x3 matrix declaration.
std::string output_glsl_mat3(const std::string &name, const Eigen::Matrix3d &m);

// Output GLSL scalar, 2-length and 3-length vector declarations.
std::string output_glsl_float(const std::string &name, float x);
std::string output_glsl_vec2(const std::string &name, float x, float y);
std::string output_glsl_vec3(const std::string &name, float x, float y, float z);

// Calculate a / b, rounding up. Does not handle overflow correctly.
unsigned div_round_up(unsigned a, unsigned b);

enum CombineRoundingBehavior {
	COMBINE_DO_NOT_ROUND = 0,
	COMBINE_ROUND_TO_FP16 = 1,
};

// Calculate where to sample, and with what weight, if one wants to use
// the GPU's bilinear hardware to sample w1 * x[pos1] + w2 * x[pos2],
// where pos1 and pos2 must be normalized coordinates describing neighboring
// texels in the mipmap level at which you sample. <num_subtexels> is the
// number of distinct accessible subtexels in the given mipmap level,
// calculated by num_texels / movit_texel_subpixel_precision. It is a float
// for performance reasons, even though it is expected to be a whole number.
// <inv_num_subtexels> is simply its inverse (1/x). <pos1_pos2_diff> is
// (pos2-pos1) and <inv_pos1_pos2_diff> is 1/(pos2-pos1).
//
// Note that since the GPU might have limited precision in its linear
// interpolation, the effective weights might be different from the ones you
// asked for. sum_sq_error, if not nullptr, will contain the sum of the
// (estimated) squared errors of the two weights.
//
// The answer, in "offset", comes as a normalized coordinate,
// so if e.g. w2 = 0, you have simply offset = pos1. If <rounding_behavior>
// is COMBINE_ROUND_TO_FP16, the coordinate is assumed to be stored as a
// rounded fp16 value. This enables more precise calculation of total_weight
// and sum_sq_error.
template<class DestFloat>
void combine_two_samples(float w1, float w2, float pos1, float pos1_pos2_diff, float inv_pos1_pos2_diff, float num_subtexels, float inv_num_subtexels,
                         DestFloat *offset, DestFloat *total_weight, float *sum_sq_error)
{
	assert(w1 * w2 >= 0.0f);  // Should not have differing signs.
	float z;  // Normalized 0..1 between pos1 and pos2.
	if (fabs(w1 + w2) < 1e-6) {
		z = 0.5f;
	} else {
		z = w2 / (w1 + w2);
	}

	// Round to the desired precision. Note that this might take z outside the 0..1 range.
	*offset = from_fp32<DestFloat>(pos1 + z * pos1_pos2_diff);
	z = (to_fp32(*offset) - pos1) * inv_pos1_pos2_diff;

	// Round to the minimum number of bits we have measured earlier.
	// The card will do this for us anyway, but if we know what the real z
	// is, we can pick a better total_weight below.
	z = lrintf(z * num_subtexels) * inv_num_subtexels;

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
	*total_weight = from_fp32<DestFloat>((w1 + z * (w2 - w1)) / (z * z + (1 - z) * (1 - z)));

	if (sum_sq_error != nullptr) {
		float err1 = to_fp32(*total_weight) * (1 - z) - w1;
		float err2 = to_fp32(*total_weight) * z - w2;
		*sum_sq_error = err1 * err1 + err2 * err2;
	}
}

// Create a VBO with the given data. Returns the VBO number.
GLuint generate_vbo(GLint size, GLenum type, GLsizeiptr data_size, const GLvoid *data);

// Create a VBO with the given data, and bind it to the vertex attribute
// with name <attribute_name>. Returns the VBO number.
GLuint fill_vertex_attribute(GLuint glsl_program_num, const std::string &attribute_name, GLint size, GLenum type, GLsizeiptr data_size, const GLvoid *data);

// Clean up after fill_vertex_attribute().
void cleanup_vertex_attribute(GLuint glsl_program_num, const std::string &attribute_name, GLuint vbo);

// If v is not already a power of two, return the first higher power of two.
unsigned next_power_of_two(unsigned v);

// Get a pointer that represents the current OpenGL context, in a cross-platform way.
// This is not intended for anything but identification (ie., so you can associate
// different FBOs with different contexts); you should probably not try to cast it
// back into anything you intend to pass into OpenGL.
void *get_gl_context_identifier();

// Used in the check_error() macro, below.
void abort_gl_error(GLenum err, const char *filename, int line) DOES_NOT_RETURN;

}  // namespace movit

#ifdef NDEBUG
#define check_error()
#else
#define check_error() { GLenum err = glGetError(); if (err != GL_NO_ERROR) { movit::abort_gl_error(err, __FILE__, __LINE__); } }
#endif

// CHECK() is like assert(), but retains any side effects no matter the compilation mode.
#ifdef NDEBUG
#define CHECK(x) (void)(x)
#else
#define CHECK(x) do { bool ok = x; if (!ok) { fprintf(stderr, "%s:%d: %s: Assertion `%s' failed.\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, #x); abort(); } } while (false)
#endif

#endif // !defined(_MOVIT_UTIL_H)
