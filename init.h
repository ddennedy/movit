#ifndef _MOVIT_INIT_H
#define _MOVIT_INIT_H

#include "defs.h"
#include <string>

namespace movit {

enum MovitDebugLevel {
	MOVIT_DEBUG_OFF = 0,
	MOVIT_DEBUG_ON = 1,
};

// Initialize the library; in particular, will query the GPU for information
// that is needed by various components. For instance, it verifies that
// we have all the OpenGL extensions we need. Returns true if initialization
// succeeded.
//
// The first parameter gives which directory to read .frag files from.
// This is a temporary hack until we add something more solid.
//
// The second parameter specifies whether debugging is on or off.
// If it is on, Movit will write intermediate graphs and the final
// generated shaders to the current directory.
//
// If you call init_movit() twice with different parameters,
// only the first will count, and the second will always return true.
bool init_movit(const std::string& data_directory, MovitDebugLevel debug_level) MUST_CHECK_RESULT;

// GPU features. These are not intended for end-user use.

// Whether init_movit() has been called.
extern bool movit_initialized;

// The current debug level.
extern MovitDebugLevel movit_debug_level;

// An estimate on the smallest values the linear texture interpolation
// of the GPU can distinguish between, i.e., for a GPU with N-bit
// texture subpixel precision, this value will be 2^-N.
//
// From reading the little specs that exist and through practical tests,
// the broad picture seems to be that Intel cards have 6-bit precision,
// nVidia cards have 8-bit, and Radeon cards have 6-bit before R6xx
// (at least when not using trilinear sampling), but can reach
// 8-bit precision on R6xx or newer in some (unspecified) cases.
//
// We currently don't bother to test for more than 1024 levels.
extern float movit_texel_subpixel_precision;

// Some GPUs use very inaccurate fixed-function circuits for rounding
// floating-point values to 8-bit outputs, leading to absurdities like
// the roundoff point between 128 and 129 being 128.62 instead of 128.5.
// We test, for every integer, x+0.48 and x+0.52 and check that they
// round the right way (giving some leeway, but not a lot); the number
// of errors are stored here.
//
// If this value is above 0, we will round off explicitly at the very end
// of the shader. Note the following limitations:
//
//   - The measurement is done on linear 8-bit, not any sRGB format,
//     10-bit output, or the likes.
//   - This only covers the final pass; intermediates are not covered
//     (only relevant if you use e.g. GL_SRGB8 intermediates).
extern int movit_num_wrongly_rounded;

// Whether the OpenGL driver (or GPU) in use supports GL_ARB_timer_query.
extern bool movit_timer_queries_supported;

// Whether the OpenGL driver (or GPU) in use supports compute shaders.
// Note that certain OpenGL implementations might only allow this in core mode.
extern bool movit_compute_shaders_supported;

// What shader model we are compiling for. This only affects the choice
// of a few files (like header.frag); most of the shaders are the same.
enum MovitShaderModel {
	MOVIT_GLSL_110,  // No longer in use, but kept until next ABI break in order not to change the enums.
	MOVIT_GLSL_130,
	MOVIT_ESSL_300,
	MOVIT_GLSL_150,
};
extern MovitShaderModel movit_shader_model;

}  // namespace movit

#endif  // !defined(_MOVIT_INIT_H)
