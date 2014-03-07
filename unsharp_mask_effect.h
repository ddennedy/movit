#ifndef _MOVIT_UNSHARP_MASK_EFFECT_H
#define _MOVIT_UNSHARP_MASK_EFFECT_H 1

// Unsharp mask is probably the most popular way of doing sharpening today,
// although it does not always deliver the best results (it is very prone
// to haloing). It simply consists of removing a blurred copy of the image from
// itself (multiplied by some strength factor). In this aspect, it's similar to
// glow, except by subtracting instead of adding.
//
// See DeconvolutionSharpenEffect for a different, possibly better
// sharpening algorithm.

#include <epoxy/gl.h>
#include <assert.h>
#include <string>

#include "effect.h"

namespace movit {

class BlurEffect;
class EffectChain;
class MixEffect;
class Node;

class UnsharpMaskEffect : public Effect {
public:
	UnsharpMaskEffect();
	virtual std::string effect_type_id() const { return "UnsharpMaskEffect"; }

	virtual bool needs_srgb_primaries() const { return false; }

	virtual void rewrite_graph(EffectChain *graph, Node *self);
	virtual bool set_float(const std::string &key, float value);

	virtual std::string output_fragment_shader() {
		assert(false);
	}
	virtual void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num) {
		assert(false);
	}

private:
	BlurEffect *blur;
	MixEffect *mix;
};

}  // namespace movit

#endif // !defined(_MOVIT_UNSHARP_MASK_EFFECT_H)
