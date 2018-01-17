#ifndef _MOVIT_BLUR_EFFECT_H
#define _MOVIT_BLUR_EFFECT_H 1

// A separable 2D blur implemented by a combination of mipmap filtering
// and convolution (essentially giving a convolution with a piecewise linear
// approximation to the true impulse response).
//
// Works in two passes; first horizontal, then vertical (BlurEffect,
// which is what the user is intended to use, instantiates two copies of
// SingleBlurPassEffect behind the scenes).
//
// The recommended number of taps is the default (16). Fewer will be faster
// but uglier; a tradeoff that might be worth it as part of more complicated
// effects. This can be set only before finalization, and must be an
// even number.

#include <epoxy/gl.h>
#include <assert.h>
#include <stddef.h>
#include <string>

#include "effect.h"

namespace movit {

class EffectChain;
class Node;
class SingleBlurPassEffect;

class BlurEffect : public Effect {
public:
	BlurEffect();

	std::string effect_type_id() const override { return "BlurEffect"; }

	void inform_input_size(unsigned input_num, unsigned width, unsigned height) override;

	std::string output_fragment_shader() override {
		assert(false);
	}
	void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num) override {
		assert(false);
	}

	void rewrite_graph(EffectChain *graph, Node *self) override;
	bool set_float(const std::string &key, float value) override;
	bool set_int(const std::string &key, int value) override;
	
private:
	void update_radius();

	int num_taps;
	float radius;
	SingleBlurPassEffect *hpass, *vpass;
	unsigned input_width, input_height;
};

class SingleBlurPassEffect : public Effect {
public:
	// If parent is non-nullptr, calls to inform_input_size will be forwarded
	// so that it can make reasonable decisions for both blur passes.
	SingleBlurPassEffect(BlurEffect *parent);
	virtual ~SingleBlurPassEffect();
	std::string effect_type_id() const override { return "SingleBlurPassEffect"; }

	std::string output_fragment_shader() override;

	// We want this for the same reason as ResizeEffect; we could end up scaling
	// down quite a lot.
	bool needs_texture_bounce() const override { return true; }
	MipmapRequirements needs_mipmaps() const override { return NEEDS_MIPMAPS; }
	bool needs_srgb_primaries() const override { return false; }
	AlphaHandling alpha_handling() const override { return INPUT_PREMULTIPLIED_ALPHA_KEEP_BLANK; }

	void inform_input_size(unsigned input_num, unsigned width, unsigned height) override {
		if (parent != nullptr) {
			parent->inform_input_size(input_num, width, height);
		}
	}
	bool changes_output_size() const override { return true; }
	bool sets_virtual_output_size() const override { return true; }
	bool one_to_one_sampling() const override { return false; }  // Can sample outside the border.

	void get_output_size(unsigned *width, unsigned *height, unsigned *virtual_width, unsigned *virtual_height) const override {
		*width = this->width;
		*height = this->height;
		*virtual_width = this->virtual_width;
		*virtual_height = this->virtual_height;
	}

	void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num) override;
	void clear_gl_state() override;
	
	enum Direction { HORIZONTAL = 0, VERTICAL = 1 };

private:
	BlurEffect *parent;
	int num_taps;
	float radius;
	Direction direction;
	int width, height, virtual_width, virtual_height;
	float *uniform_samples;
};

}  // namespace movit

#endif // !defined(_MOVIT_BLUR_EFFECT_H)
