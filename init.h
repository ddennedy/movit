#ifndef _INIT_H
#define _INIT_H

#include <string>

enum MovitDebugLevel {
	MOVIT_DEBUG_OFF = 0,
	MOVIT_DEBUG_ON = 1,
};

// Initialize the library; in particular, will query the GPU for information
// that is needed by various components. For instance, it verifies that
// we have all the OpenGL extensions we need.
//
// The first parameter gives which directory to read .frag files from.
// This is a temporary hack until we add something more solid.
//
// The second parameter specifies whether debugging is on or off.
// If it is on, Movit will write intermediate graphs and the final
// generated shaders to the current directory.
//
// If you call init_movit() twice with different parameters,
// only the first will count.
void init_movit(const std::string& data_directory, MovitDebugLevel debug_level);

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

// Whether the GPU in use supports GL_EXT_texture_sRGB.
extern bool movit_srgb_textures_supported;

#endif  // !defined(_INIT_H)
