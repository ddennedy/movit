#ifndef _UTIL_H
#define _UTIL_H 1

#include <stdio.h>
#include <stdlib.h>

#include <string>

#include <GL/gl.h>

// assumes h in [0, 2pi> or [-pi, pi>
void hsv2rgb(float h, float s, float v, float *r, float *g, float *b);

std::string read_file(const std::string &filename);
GLhandleARB compile_shader(const std::string &shader_src, GLenum type);

#ifdef NDEBUG
#define check_error()
#else
#define check_error() { int err = glGetError(); if (err != GL_NO_ERROR) { printf("GL error 0x%x at %s:%d\n", err, __FILE__, __LINE__); exit(1); } }
#endif

#endif // !defined(_UTIL_H)
