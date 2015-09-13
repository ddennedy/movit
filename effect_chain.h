#ifndef _MOVIT_EFFECT_CHAIN_H
#define _MOVIT_EFFECT_CHAIN_H 1

// An EffectChain is the largest basic entity in Movit; it contains everything
// needed to connects a series of effects, from inputs to outputs, and render
// them. Generally you set up your effect chain once and then call its render
// functions once per frame; setting one up can be relatively expensive,
// but rendering is fast.
//
// Threading considerations: EffectChain is “thread-compatible”; you can use
// different EffectChains in multiple threads at the same time (assuming the
// threads do not use the same OpenGL context, but this is a good idea anyway),
// but you may not use one EffectChain from multiple threads simultaneously.
// You _are_ allowed to use one EffectChain from multiple threads as long as
// you only use it from one at a time (possibly by doing your own locking),
// but if so, the threads' contexts need to be set up to share resources, since
// the EffectChain holds textures and other OpenGL objects that are tied to the
// context.
//
// Memory management (only relevant if you use multiple contexts):
// See corresponding comment in resource_pool.h. This holds even if you don't
// allocate your own ResourcePool, but let EffectChain hold its own.

#include <epoxy/gl.h>
#include <stdio.h>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <Eigen/Core>

#include "effect.h"
#include "image_format.h"
#include "ycbcr.h"

namespace movit {

class Effect;
class Input;
struct Phase;
class ResourcePool;

// For internal use within Node.
enum AlphaType {
	ALPHA_INVALID = -1,
	ALPHA_BLANK,
	ALPHA_PREMULTIPLIED,
	ALPHA_POSTMULTIPLIED,
};

// Whether you want pre- or postmultiplied alpha in the output
// (see effect.h for a discussion of pre- versus postmultiplied alpha).
enum OutputAlphaFormat {
	OUTPUT_ALPHA_FORMAT_PREMULTIPLIED,
	OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED,
};

// A node in the graph; basically an effect and some associated information.
class Node {
public:
	Effect *effect;
	bool disabled;

	// Edges in the graph (forward and backward).
	std::vector<Node *> outgoing_links;
	std::vector<Node *> incoming_links;

	// For unit tests only. Do not use from other code.
	// Will contain an arbitrary choice if the node is in multiple phases.
	Phase *containing_phase;

private:
	// Logical size of the output of this effect, ie. the resolution
	// you would get if you sampled it as a texture. If it is undefined
	// (since the inputs differ in resolution), it will be 0x0.
	// If both this and output_texture_{width,height} are set,
	// they will be equal.
	unsigned output_width, output_height;

	// If the effect has is_single_texture(), or if the output went to RTT
	// and that texture has been bound to a sampler, the sampler number
	// will be stored here.
	//
	// TODO: Can an RTT texture be used as inputs to multiple effects
	// within the same phase? If so, we have a problem with modifying
	// sampler state here.
	int bound_sampler_num;

	// Used during the building of the effect chain.
	Colorspace output_color_space;
	GammaCurve output_gamma_curve;
	AlphaType output_alpha_type;
	bool needs_mipmaps;  // Directly or indirectly.

	// Set if this effect, and all effects consuming output from this node
	// (in the same phase) have one_to_one_sampling() set.
	bool one_to_one_sampling;

	friend class EffectChain;
};

// A rendering phase; a single GLSL program rendering a single quad.
struct Phase {
	Node *output_node;

	GLuint glsl_program_num;  // Owned by the resource_pool.
	bool input_needs_mipmaps;

	// Inputs are only inputs from other phases (ie., those that come from RTT);
	// input textures are counted as part of <effects>.
	std::vector<Phase *> inputs;
	// Bound sampler numbers for each input. Redundant in a sense
	// (it always corresponds to the index), but we need somewhere
	// to hold the value for the uniform.
	std::vector<int> input_samplers;
	std::vector<Node *> effects;  // In order.
	unsigned output_width, output_height, virtual_output_width, virtual_output_height;

	// Identifier used to create unique variables in GLSL.
	// Unique per-phase to increase cacheability of compiled shaders.
	std::map<Node *, std::string> effect_ids;

	// Uniforms for this phase; combined from all the effects.
	std::vector<Uniform<int> > uniforms_sampler2d;
	std::vector<Uniform<bool> > uniforms_bool;
	std::vector<Uniform<int> > uniforms_int;
	std::vector<Uniform<float> > uniforms_float;
	std::vector<Uniform<float> > uniforms_vec2;
	std::vector<Uniform<float> > uniforms_vec3;
	std::vector<Uniform<float> > uniforms_vec4;
	std::vector<Uniform<Eigen::Matrix3d> > uniforms_mat3;

	// For measurement of GPU time used.
	GLuint timer_query_object;
	uint64_t time_elapsed_ns;
	uint64_t num_measured_iterations;
};

class EffectChain {
public:
	// Aspect: e.g. 16.0f, 9.0f for 16:9.
	// resource_pool is a pointer to a ResourcePool with which to share shaders
	// and other resources (see resource_pool.h). If NULL (the default),
	// will create its own that is not shared with anything else. Does not take
	// ownership of the passed-in ResourcePool, but will naturally take ownership
	// of its own internal one if created.
	EffectChain(float aspect_nom, float aspect_denom, ResourcePool *resource_pool = NULL);
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
	Effect *add_effect(Effect *effect, Effect *input1, Effect *input2, Effect *input3) {
		std::vector<Effect *> inputs;
		inputs.push_back(input1);
		inputs.push_back(input2);
		inputs.push_back(input3);
		return add_effect(effect, inputs);
	}
	Effect *add_effect(Effect *effect, const std::vector<Effect *> &inputs);

	// Adds an RGB output. Note that you can only have one output.
	void add_output(const ImageFormat &format, OutputAlphaFormat alpha_format);

	// Adds an YCbCr output. Note that you can only have one output.
	// Currently, only chunked packed output is supported, and only 4:4:4
	// (so chroma_subsampling_x and chroma_subsampling_y must both be 1).
	void add_ycbcr_output(const ImageFormat &format, OutputAlphaFormat alpha_format,
	                      const YCbCrFormat &ycbcr_format);

	// Set number of output bits, to scale the dither.
	// 8 is the right value for most outputs.
	// The default, 0, is a special value that means no dither.
	void set_dither_bits(unsigned num_bits)
	{
		this->num_dither_bits = num_bits;
	}

	void finalize();

	// Measure the GPU time used for each actual phase during rendering.
	// Note that this is only available if GL_ARB_timer_query
	// (or, equivalently, OpenGL 3.3) is available. Also note that measurement
	// will incur a performance cost, as we wait for the measurements to
	// complete at the end of rendering.
	void enable_phase_timing(bool enable);
	void reset_phase_timing();
	void print_phase_timing();

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
	Node *find_node_for_effect(Effect *effect) { return node_map[effect]; }

	// Get the OpenGL sampler (GL_TEXTURE0, GL_TEXTURE1, etc.) for the
	// input of the given node, so that one can modify the sampler state
	// directly. Only valid to call during set_gl_state().
	//
	// Also, for this to be allowed, <node>'s effect must have
	// needs_texture_bounce() set, so that it samples directly from a
	// single-sampler input, or from an RTT texture.
	GLenum get_input_sampler(Node *node, unsigned input_num) const;

	// Get the current resource pool assigned to this EffectChain.
	// Primarily to let effects allocate textures as needed.
	// Any resources you get from the pool must be returned to the pool
	// no later than in the Effect's destructor.
	ResourcePool *get_resource_pool() { return resource_pool; }

private:
	// Make sure the output rectangle is at least large enough to hold
	// the given input rectangle in both dimensions, and is of the
	// current aspect ratio (aspect_nom/aspect_denom).
	void size_rectangle_to_fit(unsigned width, unsigned height, unsigned *output_width, unsigned *output_height);

	// Compute the input sizes for all inputs for all effects in a given phase,
	// and inform the effects about the results.	
	void inform_input_sizes(Phase *phase);

	// Determine the preferred output size of a given phase.
	// Requires that all input phases (if any) already have output sizes set.
	void find_output_size(Phase *phase);

	// Find all inputs eventually feeding into this effect that have
	// output gamma different from GAMMA_LINEAR.
	void find_all_nonlinear_inputs(Node *effect, std::vector<Node *> *nonlinear_inputs);

	// Create a GLSL program computing the effects for this phase in order.
	void compile_glsl_program(Phase *phase);

	// Create all GLSL programs needed to compute the given effect, and all outputs
	// that depend on it (whenever possible). Returns the phase that has <output>
	// as the last effect. Also pushes all phases in order onto <phases>.
	Phase *construct_phase(Node *output, std::map<Node *, Phase *> *completed_effects);

	// Execute one phase, ie. set up all inputs, effects and outputs, and render the quad.
	void execute_phase(Phase *phase, bool last_phase, std::map<Phase *, GLuint> *output_textures, std::set<Phase *> *generated_mipmaps);

	// Set up uniforms for one phase. The program must already be bound.
	void setup_uniforms(Phase *phase);

	// Set up the given sampler number for sampling from an RTT texture.
	void setup_rtt_sampler(int sampler_num, bool use_mipmaps);

	// Output the current graph to the given file in a Graphviz-compatible format;
	// only useful for debugging.
	void output_dot(const char *filename);
	std::vector<std::string> get_labels_for_edge(const Node *from, const Node *to);
	void output_dot_edge(FILE *fp,
	                     const std::string &from_node_id,
	                     const std::string &to_node_id,
			     const std::vector<std::string> &labels);

	// Some of the graph algorithms assume that the nodes array is sorted
	// topologically (inputs are always before outputs), but some operations
	// (like graph rewriting) can change that. This function restores that order.
	void sort_all_nodes_topologically();

	// Do the actual topological sort. <nodes> must be a connected, acyclic subgraph;
	// links that go to nodes not in the set will be ignored.
	std::vector<Node *> topological_sort(const std::vector<Node *> &nodes);

	// Utility function used by topological_sort() to do a depth-first search.
	// The reason why we store nodes left to visit instead of a more conventional
	// list of nodes to visit is that we want to be able to limit ourselves to
	// a subgraph instead of all nodes. The set thus serves a dual purpose.
	void topological_sort_visit_node(Node *node, std::set<Node *> *nodes_left_to_visit, std::vector<Node *> *sorted_list);

	// Used during finalize().
	void find_color_spaces_for_inputs();
	void propagate_alpha();
	void propagate_gamma_and_color_space();
	Node *find_output_node();

	bool node_needs_colorspace_fix(Node *node);
	void fix_internal_color_spaces();
	void fix_output_color_space();

	bool node_needs_alpha_fix(Node *node);
	void fix_internal_alpha(unsigned step);
	void fix_output_alpha();

	bool node_needs_gamma_fix(Node *node);
	void fix_internal_gamma_by_asking_inputs(unsigned step);
	void fix_internal_gamma_by_inserting_nodes(unsigned step);
	void fix_output_gamma();
	void add_ycbcr_conversion_if_needed();
	void add_dither_if_needed();

	float aspect_nom, aspect_denom;
	ImageFormat output_format;
	OutputAlphaFormat output_alpha_format;

	enum OutputColorType { OUTPUT_COLOR_RGB, OUTPUT_COLOR_YCBCR };
	OutputColorType output_color_type;
	YCbCrFormat output_ycbcr_format;  // If output_color_type == OUTPUT_COLOR_YCBCR.

	std::vector<Node *> nodes;
	std::map<Effect *, Node *> node_map;
	Effect *dither_effect;

	std::vector<Input *> inputs;  // Also contained in nodes.
	std::vector<Phase *> phases;

	unsigned num_dither_bits;
	bool finalized;

	ResourcePool *resource_pool;
	bool owns_resource_pool;

	bool do_phase_timing;
};

}  // namespace movit

#endif // !defined(_MOVIT_EFFECT_CHAIN_H)
