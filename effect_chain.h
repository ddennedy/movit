#ifndef _EFFECT_CHAIN_H
#define _EFFECT_CHAIN_H 1

#include <set>
#include <vector>

#include "effect.h"
#include "image_format.h"
#include "input.h"

class EffectChain {
public:
	EffectChain(unsigned width, unsigned height);

	// User API:
	// input, effects, output, finalize need to come in that specific order.

	// EffectChain takes ownership of the given input.
	// input is returned back for convenience.
	Input *add_input(Input *input);

	// EffectChain takes ownership of the given effect.
	// effect is returned back for convenience.
	Effect *add_effect(Effect *effect) {
		return add_effect(effect, last_added_effect());
	}
	Effect *add_effect(Effect *effect, Effect *input) {
		std::vector<Effect *> inputs;
		inputs.push_back(input);
		return add_effect(effect, inputs);
	}
	Effect *add_effect(Effect *effect, Effect *input1, Effect *input2) {
		std::vector<Effect *> inputs;
		inputs.push_back(input1);
		inputs.push_back(input2);
		return add_effect(effect, inputs);
	}
	Effect *add_effect(Effect *effect, const std::vector<Effect *> &inputs);

	// Similar to add_effect, but:
	//
	//  * Does not insert any normalizing effects.
	//  * Does not ask the effect to insert itself, so it won't work
	//    with meta-effects.
	//
	// We should really separate out these two “sides” of Effect in the
	// type system soon.
	void add_effect_raw(Effect *effect, const std::vector<Effect *> &inputs);

	void add_output(const ImageFormat &format);
	void finalize();

	//void render(unsigned char *src, unsigned char *dst);
	void render_to_screen();

	Effect *last_added_effect() {
		if (effects.empty()) {
			return NULL;
		} else {
			return effects.back();
		}	
	}

private:
	struct Phase {
		GLint glsl_program_num;
		bool input_needs_mipmaps;
		std::vector<Effect *> inputs;   // Only from other phases; input textures are not counted here.
		std::vector<Effect *> effects;  // In order.
	};

	void find_all_nonlinear_inputs(Effect *effect,
	                               std::vector<Input *> *nonlinear_inputs,
	                               std::vector<Effect *> *intermediates);
	Effect *normalize_to_linear_gamma(Effect *input);
	Effect *normalize_to_srgb(Effect *input);

	void draw_vertex(float x, float y, const std::vector<Effect *> &inputs);

	// Create a GLSL program computing the given effects in order.
	Phase compile_glsl_program(const std::vector<Effect *> &inputs, const std::vector<Effect *> &effects);

	// Create all GLSL programs needed to compute the given effect, and all outputs
	// that depends on it (whenever possible).
	void construct_glsl_programs(Effect *output);

	unsigned width, height;
	ImageFormat output_format;
	std::vector<Effect *> effects;
	std::vector<Input *> inputs;  // Also contained in effects.
	std::map<Effect *, std::string> effect_ids;
	std::map<Effect *, GLuint> effect_output_textures;
	std::map<Effect *, std::vector<Effect *> > outgoing_links;
	std::map<Effect *, std::vector<Effect *> > incoming_links;

	GLuint fbo;
	std::vector<Phase> phases;

	GLenum format, bytes_per_pixel;
	bool finalized;

	// Used during the building of the effect chain.
	std::map<Effect *, ColorSpace> output_color_space;
	std::map<Effect *, GammaCurve> output_gamma_curve;	
};


#endif // !defined(_EFFECT_CHAIN_H)
