#ifndef _UTIL_H
#define _UTIL_H 1

#include <stdio.h>
#include <stdlib.h>

#include <string>

#include <GL/gl.h>

// assumes h in [0, 2pi> or [-pi, pi>
void hsv2rgb(float h, float s, float v, float *r, float *g, float *b);

// Column major (same as OpenGL).
typedef double Matrix3x3[9];

std::string read_file(const std::string &filename);
GLhandleARB compile_shader(const std::string &shader_src, GLenum type);
void multiply_3x3_matrices(const Matrix3x3 a, const Matrix3x3 b, Matrix3x3 result);
void invert_3x3_matrix(const Matrix3x3 m, Matrix3x3 result);
void print_3x3_matrix(const Matrix3x3 m);

#ifdef NDEBUG
#define check_error()
#else
#define check_error() { int err = glGetError(); if (err != GL_NO_ERROR) { printf("GL error 0x%x at %s:%d\n", err, __FILE__, __LINE__); exit(1); } }
#endif

#endif // !defined(_UTIL_H)
