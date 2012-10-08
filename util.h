#ifndef _UTIL_H
#define _UTIL_H 1

// Various utilities.

#include <stdio.h>
#include <stdlib.h>

#include <string>

#include "opengl.h"

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

// Converts a HSV color to RGB. Assumes h in [0, 2pi> or [-pi, pi>
void hsv2rgb(float h, float s, float v, float *r, float *g, float *b);

// Column major (same as OpenGL).
typedef double Matrix3x3[9];

// Read a file from disk and return its contents.
// Dies if the file does not exist.
std::string read_file(const std::string &filename);

// Compile the given GLSL shader (typically a vertex or fragment shader)
// and return the object number.
GLuint compile_shader(const std::string &shader_src, GLenum type);

// Compute a * b.
void multiply_3x3_matrices(const Matrix3x3 a, const Matrix3x3 b, Matrix3x3 result);

// Compute M * [x0 x1 x2]'.
void multiply_3x3_matrix_float3(const Matrix3x3 M, float x0, float x1, float x2, float *y0, float *y1, float *y2);

// Compute m^-1. Result is undefined if the matrix is singular or near-singular.
void invert_3x3_matrix(const Matrix3x3 m, Matrix3x3 result);

// Print a 3x3 matrix to standard output. Useful for debugging.
void print_3x3_matrix(const Matrix3x3 m);

#ifdef NDEBUG
#define check_error()
#else
#define check_error() { int err = glGetError(); if (err != GL_NO_ERROR) { printf("GL error 0x%x at %s:%d\n", err, __FILE__, __LINE__); exit(1); } }
#endif

#endif // !defined(_UTIL_H)
