#ifndef _EFFECT_CHAIN_H
#define _EFFECT_CHAIN_H 1

#include <set>
#include <vector>

#include "effect.h"
#include "image_format.h"
#include "input.h"

class EffectChain;
class Phase;

// A node in the graph; basically an effect and some associated information.
class Node {
public:
	Effect *effect;
	bool disabled;

	// Edges in the graph (forward and backward).
	std::vector<Node *> outgoing_links;
	std::vector<Node *> incoming_links;

private:
	// Identifier used to create unique variables in GLSL.
	std::string effect_id;

	// Logical size of the output of this effect, ie. the resolution
	// you would get if you sampled it as a texture. If it is undefined
	// (since the inputs differ in resolution), it will be 0x0.
	// If both this and output_texture_{width,height} are set,
	// they will be equal.
	unsigned output_width, output_height;

	// If output goes to RTT (otherwise, none of these are set).
	// The Phase pointer is a but ugly; we should probably fix so
	// that Phase takes other phases as inputs, instead of Node.
	GLuint output_texture;
	unsigned output_texture_width, output_texture_height;
	Phase *phase;

	// Used during the building of the effect chain.
	Colorspace output_color_space;
	GammaCurve output_gamma_curve;

	friend class EffectChain;
};

// A rendering phase; a single GLSL program rendering a single quad.
struct Phase {
	GLint glsl_program_num, vertex_shader, fragment_shader;
	bool input_needs_mipmaps;

	// Inputs are only inputs from other phases (ie., those that come from RTT);
	// input textures are not counted here.
	std::vector<Node *> inputs;

	std::vector<Node *> effects;  // In order.
	unsigned output_width, output_height;
};

class EffectChain {
public:
	EffectChain(float aspect_nom, float aspect_denom);  // E.g., 16.0f, 9.0f for 16:9.
	~EffectChain();

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

	void add_output(const ImageFormat &format);

	// Set number of output bits, to scale the dither.
	// 8 is the right value for most outputs.
	// The default, 0, is a special value that means no dither.
	void set_dither_bits(unsigned num_bits)
	{
		this->num_dither_bits = num_bits;
	}

	void finalize();


	//void render(unsigned char *src, unsigned char *dst);
	void render_to_screen()
	{
		render_to_fbo(0, 0, 0);
	}

	// Render the effect chain to the given FBO. If width=height=0, keeps
	// the current viewport.
	void render_to_fbo(GLuint fbo, unsigned width, unsigned height);

	Effect *last_added_effect() {
		if (nodes.empty()) {
			return NULL;
		} else {
			return nodes.back()->effect;
		}	
	}

	// API for manipulating the graph directly. Intended to be used from
	// effects and by EffectChain itself.
	//
	// Note that for nodes with multiple inputs, the order of calls to
	// connect_nodes() will matter.
	Node *add_node(Effect *effect);
	void connect_nodes(Node *sender, Node *receiver);
	void replace_receiver(Node *old_receiver, Node *new_receiver);
	void replace_sender(Node *new_sender, Node *receiver);
	void insert_node_between(Node *sender, Node *middle, Node *receiver);

private:
	// Fits a rectangle of the given size to the current aspect ratio
	// (aspect_nom/aspect_denom) and returns the new width and height.
	unsigned fit_rectangle_to_aspect(unsigned width, unsigned height);

	// Compute the input sizes for all inputs for all effects in a given phase,
	// and inform the effects about the results.	
	void inform_input_sizes(Phase *phase);

	// Determine the preferred output size of a given phase.
	// Requires that all input phases (if any) already have output sizes set.
	void find_output_size(Phase *phase);

	// Find all inputs eventually feeding into this effect that have
	// output gamma different from GAMMA_LINEAR.
	void find_all_nonlinear_inputs(Node *effect, std::vector<Node *> *nonlinear_inputs);

	// Create a GLSL program computing the given effects in order.
	Phase *compile_glsl_program(const std::vector<Node *> &inputs,
	                            const std::vector<Node *> &effects);

	// Create all GLSL programs needed to compute the given effect, and all outputs
	// that depends on it (whenever possible).
	void construct_glsl_programs(Node *output);

	// Output the current graph to the given file in a Graphviz-compatible format;
	// only useful for debugging.
	void output_dot(const char *filename);

	// Some of the graph algorithms assume that the nodes array is sorted
	// topologically (inputs are always before outputs), but some operations
	// (like graph rewriting) can change that. This function restores that order.
	void sort_nodes_topologically();
	void topological_sort_visit_node(Node *node, std::set<Node *> *visited_nodes, std::vector<Node *> *sorted_list);

	// Used during finalize().
	void propagate_gamma_and_color_space();
	Node *find_output_node();

	bool node_needs_colorspace_fix(Node *node);
	void fix_internal_color_spaces();
	void fix_output_color_space();

	bool node_needs_gamma_fix(Node *node);
	void fix_internal_gamma_by_asking_inputs(unsigned step);
	void fix_internal_gamma_by_inserting_nodes(unsigned step);
	void fix_output_gamma();
	void add_dither_if_needed();

	float aspect_nom, aspect_denom;
	ImageFormat output_format;

	std::vector<Node *> nodes;
	std::map<Effect *, Node *> node_map;
	Effect *dither_effect;

	std::vector<Input *> inputs;  // Also contained in nodes.

	GLuint fbo;
	std::vector<Phase *> phases;

	GLenum format;
	unsigned bytes_per_pixel, num_dither_bits;
	bool finalized;
};

#endif // !defined(_EFFECT_CHAIN_H)
