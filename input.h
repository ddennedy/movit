#ifndef _INPUT_H
#define _INPUT_H 1

#include <assert.h>

#include "effect.h"
#include "image_format.h"

// An input is a degenerate case of an effect; it represents the picture data
// that comes from the user. As such, it has zero “inputs” itself. Also, it
// has an extra operation called finalize(), which runs when the effect chain
// is finalized.
//
// An input is, like any other effect, required to be able to output a GLSL
// fragment giving a RGBA value (although that GLSL fragment will have zero
// inputs itself), and set the required OpenGL state on set_gl_state(),
// including possibly uploading the texture if so required.
class Input : public Effect {
public:
	virtual unsigned num_inputs() const { return 0; }

	// Create the texture itself. We cannot do this in the constructor,
	// because we don't necessarily know all the settings (sRGB texture,
	// mipmap generation) at that point.
	virtual void finalize() = 0;

	virtual ColorSpace get_color_space() = 0;	
	virtual GammaCurve get_gamma_curve() = 0;	
};

#endif // !defined(_INPUT_H)
