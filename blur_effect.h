#ifndef _BLUR_EFFECT_H
#define _BLUR_EFFECT_H 1

// A separable 2D blur implemented by a combination of mipmap filtering
// and convolution (essentially giving a convolution with a piecewise linear
// approximation to the true impulse response).
//
// Works in two passes; first horizontal, then vertical (BlurEffect,
// which is what the user is intended to use, instantiates two copies of
// SingleBlurPassEffect behind the scenes).

#include "effect.h"

class SingleBlurPassEffect;

class BlurEffect : public Effect {
public:
	BlurEffect();

	virtual std::string effect_type_id() const { return "BlurEffect"; }

	// We want this for the same reason as ResizeEffect; we could end up scaling
	// down quite a lot.
	virtual bool needs_texture_bounce() const { return true; }
	virtual bool needs_mipmaps() const { return true; }
	virtual bool needs_srgb_primaries() const { return false; }
	virtual AlphaHandling alpha_handling() const { return INPUT_AND_OUTPUT_ALPHA_PREMULTIPLIED; }

	virtual void inform_input_size(unsigned input_num, unsigned width, unsigned height);

	virtual std::string output_fragment_shader() {
		assert(false);
	}
	virtual void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num) {
		assert(false);
	}

	virtual void rewrite_graph(EffectChain *graph, Node *self);
	virtual bool set_float(const std::string &key, float value);
	
private:
	void update_radius();
	
	float radius;
	SingleBlurPassEffect *hpass, *vpass;
	unsigned input_width, input_height;
};

class SingleBlurPassEffect : public Effect {
public:
	// If parent is non-NULL, calls to inform_input_size will be forwarded
	// so that it can make reasonable decisions for both blur passes.
	SingleBlurPassEffect(BlurEffect *parent);
	virtual std::string effect_type_id() const { return "SingleBlurPassEffect"; }

	std::string output_fragment_shader();

	virtual bool needs_texture_bounce() const { return true; }
	virtual bool needs_mipmaps() const { return true; }
	virtual bool needs_srgb_primaries() const { return false; }

	virtual void inform_input_size(unsigned input_num, unsigned width, unsigned height) {
		if (parent != NULL) {
			parent->inform_input_size(input_num, width, height);
		}
	}
	virtual bool changes_output_size() const { return true; }

	virtual void get_output_size(unsigned *width, unsigned *height) const {
		*width = this->width;
		*height = this->height;
	}

	void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num);
	void clear_gl_state();
	
	enum Direction { HORIZONTAL = 0, VERTICAL = 1 };

private:
	BlurEffect *parent;
	float radius;
	Direction direction;
	int width, height;
};

#endif // !defined(_BLUR_EFFECT_H)
