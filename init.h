#ifndef _INIT_H
#define _INIT_H

// Initialize the library; in particular, will query the GPU for information
// that is needed by various components. (In time, for instance, we will query
// about extensions here.)
void init_movit();

// GPU features. These are not intended for end-user use.

// Whether init_movit() has been called.
extern bool movit_initialized;

// An estimate on the number of different levels the linear texture interpolation
// of the GPU can deliver. My Intel card seems to be limited to 2^6 levels here,
// while a modern nVidia card (GTX 550 Ti) seem to use 2^8.
//
// We currently don't bother to test above 2^10.
extern float movit_texel_subpixel_precision;

#endif  // !defined(_INIT_H)
