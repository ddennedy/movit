#ifndef _INIT_H
#define _INIT_H

// Initialize the library; in particular, will query the GPU for information
// that is needed by various components. For instance, it verifies that
// we have all the OpenGL extensions we need.
void init_movit();

// GPU features. These are not intended for end-user use.

// Whether init_movit() has been called.
extern bool movit_initialized;

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
