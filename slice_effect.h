#ifndef _MOVIT_SLICE_EFFECT_H
#define _MOVIT_SLICE_EFFECT_H 1

// SliceEffect takes an image, cuts it into (potentially overlapping) slices,
// and puts those slices back together again consecutively. It is primarily
// useful in an overlap-discard setting, where it can do both the overlap and
// discard roles, where one does convolutions by means of many small FFTs, but
// could also work as a (relatively boring) video effect on its own.
//
// Note that vertical slices happen from the bottom, not the top, due to the
// OpenGL coordinate system.

#include <GL/glew.h>
#include <string>

#include "effect.h"

namespace movit {

class SliceEffect : public Effect {
public:
	SliceEffect();
	virtual std::string effect_type_id() const { return "SliceEffect"; }
	std::string output_fragment_shader();
	virtual bool needs_texture_bounce() const { return true; }
	virtual bool changes_output_size() const { return true; }
	virtual void inform_input_size(unsigned input_num, unsigned width, unsigned height);
	virtual void get_output_size(unsigned *width, unsigned *height,
	                             unsigned *virtual_width, unsigned *virtual_height) const;

	void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num);
	virtual void inform_added(EffectChain *chain) { this->chain = chain; }
	
	enum Direction { HORIZONTAL = 0, VERTICAL = 1 };

private:
	EffectChain *chain;
	int input_width, input_height;
	int input_slice_size, output_slice_size;
	Direction direction;
};

}  // namespace movit

#endif // !defined(_MOVIT_SLICE_EFFECT_H)
