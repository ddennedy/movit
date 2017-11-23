#ifndef _MOVIT_INPUT_H
#define _MOVIT_INPUT_H 1

#include <assert.h>

#include "effect.h"
#include "image_format.h"

namespace movit {

// An input is a degenerate case of an effect; it represents the picture data
// that comes from the user. As such, it has zero “inputs” itself.
//
// An input is, like any other effect, required to be able to output a GLSL
// fragment giving a RGBA value (although that GLSL fragment will have zero
// inputs itself), and set the required OpenGL state on set_gl_state(),
// including possibly uploading the texture if so required.
class Input : public Effect {
public:
	unsigned num_inputs() const override { return 0; }

	// Whether this input can deliver linear gamma directly if it's
	// asked to. (If so, set the parameter “output_linear_gamma”
	// to activate it.)
	virtual bool can_output_linear_gamma() const = 0;

	// Whether this input can supply mipmaps if asked to (by setting
	// the "needs_mipmaps" integer parameter set to 1).
	virtual bool can_supply_mipmaps() const { return true; }

	virtual unsigned get_width() const = 0;
	virtual unsigned get_height() const = 0;
	virtual Colorspace get_color_space() const = 0;
	virtual GammaCurve get_gamma_curve() const = 0;
};

}  // namespace movit

#endif // !defined(_MOVIT_INPUT_H)
