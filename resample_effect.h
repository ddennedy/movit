#ifndef _MOVIT_RESAMPLE_EFFECT_H
#define _MOVIT_RESAMPLE_EFFECT_H 1

// High-quality image resizing, either up or down.
//
// The default scaling offered by the GPU (and as used in ResizeEffect)
// is bilinear (optionally mipmapped), which is not the highest-quality
// choice, especially for upscaling. ResampleEffect offers the three-lobed
// Lanczos kernel, which is among the most popular choices in image
// processing. While it does have its weaknesses, in particular a certain
// ringing/sharpening effect with artifacts that accumulate over several
// consecutive resizings, it is generally regarded as the best tradeoff.
//
// Works in two passes; first horizontal, then vertical (ResampleEffect,
// which is what the user is intended to use, instantiates two copies of
// SingleResamplePassEffect behind the scenes).

#include <epoxy/gl.h>
#include <assert.h>
#include <stddef.h>
#include <string>

#include "effect.h"

namespace movit {

class EffectChain;
class Node;
class SingleResamplePassEffect;

class ResampleEffect : public Effect {
public:
	ResampleEffect();
	virtual ~ResampleEffect();

	virtual std::string effect_type_id() const { return "ResampleEffect"; }

	// We want this for the same reason as ResizeEffect; we could end up scaling
	// down quite a lot.
	virtual bool needs_texture_bounce() const { return true; }
	virtual bool needs_srgb_primaries() const { return false; }

	virtual void inform_input_size(unsigned input_num, unsigned width, unsigned height);

	virtual std::string output_fragment_shader();

	virtual void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num) {
		assert(false);
	}

	virtual void rewrite_graph(EffectChain *graph, Node *self);
	virtual bool set_float(const std::string &key, float value);
	
private:
	void update_size();
	void update_offset_and_zoom();
	
	SingleResamplePassEffect *hpass, *vpass;
	int input_width, input_height, output_width, output_height;

	float offset_x, offset_y;
	float zoom_x, zoom_y;
	float zoom_center_x, zoom_center_y;
	bool is_used;
};

class SingleResamplePassEffect : public Effect {
public:
	// If parent is non-NULL, calls to inform_input_size will be forwarded,
	// so that it can inform both passes about the right input and output
	// resolutions.
	SingleResamplePassEffect(ResampleEffect *parent);
	~SingleResamplePassEffect();
	virtual std::string effect_type_id() const { return "SingleResamplePassEffect"; }

	std::string output_fragment_shader();

	virtual bool needs_texture_bounce() const { return true; }
	virtual bool needs_srgb_primaries() const { return false; }
	virtual AlphaHandling alpha_handling() const { return INPUT_PREMULTIPLIED_ALPHA_KEEP_BLANK; }

	virtual void inform_added(EffectChain *chain) { this->chain = chain; }
	virtual void inform_input_size(unsigned input_num, unsigned width, unsigned height) {
		if (parent != NULL) {
			parent->inform_input_size(input_num, width, height);
		}
	}
	virtual bool changes_output_size() const { return true; }
	virtual bool sets_virtual_output_size() const { return false; }

	virtual void get_output_size(unsigned *width, unsigned *height, unsigned *virtual_width, unsigned *virtual_height) const {
		*virtual_width = *width = this->output_width;
		*virtual_height = *height = this->output_height;
	}

	void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num);
	
	enum Direction { HORIZONTAL = 0, VERTICAL = 1 };

private:
	void update_texture(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num);

	ResampleEffect *parent;
	EffectChain *chain;
	Direction direction;
	GLuint texnum;
	GLint uniform_sample_tex;
	float uniform_num_loops, uniform_slice_height, uniform_sample_x_scale, uniform_sample_x_offset;
	float uniform_whole_pixel_offset;
	int uniform_num_samples;

	int input_width, input_height, output_width, output_height;
	float offset, zoom;
	int last_input_width, last_input_height, last_output_width, last_output_height;
	float last_offset, last_zoom;
	int src_bilinear_samples, num_loops;
	float slice_height;
	int last_texture_width, last_texture_height;
	GLuint last_texture_internal_format;
};

}  // namespace movit

#endif // !defined(_MOVIT_RESAMPLE_EFFECT_H)
