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
		if (nodes.empty()) {
			return NULL;
		} else {
			return nodes.back()->effect;
		}	
	}

private:
	struct Phase;

	// A node in the graph; basically an effect and some associated information.
	struct Node {
		Effect *effect;

		// Identifier used to create unique variables in GLSL.
		std::string effect_id;

		// Edges in the graph (forward and backward).
		std::vector<Node *> outgoing_links;
		std::vector<Node *> incoming_links;

		// If output goes to RTT (otherwise, none of these are set).
		// The Phsae pointer is a but ugly; we should probably fix so
		// that Phase takes other phases as inputs, instead of Node.
		GLuint output_texture;
		unsigned output_texture_width, output_texture_height;
		Phase *phase;

		// Used during the building of the effect chain.
		ColorSpace output_color_space;
		GammaCurve output_gamma_curve;	
	};

	// A rendering phase; a single GLSL program rendering a single quad.
	struct Phase {
		GLint glsl_program_num;
		bool input_needs_mipmaps;

		// Inputs are only inputs from other phases (ie., those that come from RTT);
		// input textures are not counted here.
		std::vector<Node *> inputs;

		std::vector<Node *> effects;  // In order.
		unsigned output_width, output_height;
	};

	// Determine the preferred output size of a given phase.
	// Requires that all input phases (if any) already have output sizes set.
	void find_output_size(Phase *phase);

	void find_all_nonlinear_inputs(Node *effect,
	                               std::vector<Node *> *nonlinear_inputs,
	                               std::vector<Node *> *intermediates);
	Node *normalize_to_linear_gamma(Node *input);
	Node *normalize_to_srgb(Node *input);

	void draw_vertex(float x, float y, const std::vector<Effect *> &inputs);

	// Create a GLSL program computing the given effects in order.
	Phase *compile_glsl_program(const std::vector<Node *> &inputs,
	                            const std::vector<Node *> &effects);

	// Create all GLSL programs needed to compute the given effect, and all outputs
	// that depends on it (whenever possible).
	void construct_glsl_programs(Node *output);

	unsigned width, height;
	ImageFormat output_format;

	std::vector<Node *> nodes;
	std::map<Effect *, Node *> node_map;

	std::vector<Input *> inputs;  // Also contained in nodes.

	GLuint fbo;
	std::vector<Phase *> phases;

	GLenum format, bytes_per_pixel;
	bool finalized;
};


#endif // !defined(_EFFECT_CHAIN_H)
