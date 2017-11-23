#ifndef _MOVIT_SLICE_EFFECT_H
#define _MOVIT_SLICE_EFFECT_H 1

// SliceEffect takes an image, cuts it into (potentially overlapping) slices,
// and puts those slices back together again consecutively. It is primarily
// useful in an overlap-discard setting, where it can do both the overlap and
// discard roles, where one does convolutions by means of many small FFTs, but
// could also work as a (relatively boring) video effect on its own.
//
// Note that vertical slices happen from the top, consistent with the rest of
// Movit.

#include <epoxy/gl.h>
#include <string>

#include "effect.h"

namespace movit {

class SliceEffect : public Effect {
public:
	SliceEffect();
	std::string effect_type_id() const override { return "SliceEffect"; }
	std::string output_fragment_shader() override;
	bool needs_texture_bounce() const override { return true; }
	bool changes_output_size() const override { return true; }
	bool sets_virtual_output_size() const override { return false; }
	void inform_input_size(unsigned input_num, unsigned width, unsigned height) override;
	void get_output_size(unsigned *width, unsigned *height,
	                     unsigned *virtual_width, unsigned *virtual_height) const override;

	void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num) override;
	void inform_added(EffectChain *chain) override { this->chain = chain; }
	
	enum Direction { HORIZONTAL = 0, VERTICAL = 1 };

private:
	EffectChain *chain;
	int input_width, input_height;
	int input_slice_size, output_slice_size;
	int offset;
	Direction direction;

	float uniform_output_coord_to_slice_num, uniform_slice_num_to_input_coord;
	float uniform_slice_offset_to_input_coord, uniform_offset;
};

}  // namespace movit

#endif // !defined(_MOVIT_SLICE_EFFECT_H)
