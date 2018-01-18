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
#include <list>
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

// RGBA output is nearly always packed; Y'CbCr, however, is often planar
// due to chroma subsampling. This enum controls how add_ycbcr_output()
// distributes the color channels between the fragment shader outputs.
// Obviously, anything except YCBCR_OUTPUT_INTERLEAVED will be meaningless
// unless you use render_to_fbo() and have an FBO with multiple render
// targets attached (the other outputs will be discarded).
enum YCbCrOutputSplitting {
	// Only one output: Store Y'CbCr into the first three output channels,
	// respectively, plus alpha. This is also called “chunked” or
	// ”packed” mode.
	YCBCR_OUTPUT_INTERLEAVED,

	// Store Y' and alpha into the first output (in the red and alpha
	// channels; effect to the others is undefined), and Cb and Cr into
	// the first two channels of the second output. This is particularly
	// useful if you want to end up in a format like NV12, where all the
	// Y' samples come first and then Cb and Cr come interlevaed afterwards.
	// You will still need to do the chroma subsampling yourself to actually
	// get down to NV12, though.
	YCBCR_OUTPUT_SPLIT_Y_AND_CBCR,

	// Store Y' and alpha into the first output, Cb into the first channel
	// of the second output and Cr into the first channel of the third output.
	// (Effect on the other channels is undefined.) Essentially gives you
	// 4:4:4 planar, or ”yuv444p”.
	YCBCR_OUTPUT_PLANAR,
};

// Where (0,0) is taken to be in the output. If you want to render to an
// OpenGL screen, you should keep the default of bottom-left, as that is
// OpenGL's natural coordinate system. However, there are cases, such as if you
// render to an FBO and read the pixels back into some other system, where
// you'd want a top-left origin; if so, an additional flip step will be added
// at the very end (but done in a vertex shader, so it will have zero extra
// cost).
//
// Note that Movit's coordinate system in general consistently puts (0,0) in
// the top left for _input_, no matter what you set as output origin.
enum OutputOrigin {
	OUTPUT_ORIGIN_BOTTOM_LEFT,
	OUTPUT_ORIGIN_TOP_LEFT,
};

// Transformation to apply (if any) to pixel data in temporary buffers.
// See set_intermediate_format() below for more information.
enum FramebufferTransformation {
	// The default; just store the value. This is what you usually want.
	NO_FRAMEBUFFER_TRANSFORMATION,

	// If the values are in linear light, store sqrt(x) to the framebuffer
	// instead of x itself, of course undoing it with x² on read. Useful as
	// a rough approximation to the sRGB curve. (If the values are not in
	// linear light, just store them as-is.)
	SQUARE_ROOT_FRAMEBUFFER_TRANSFORMATION,
};

// Whether a link is into another phase or not; see Node::incoming_link_type.
enum NodeLinkType {
	IN_ANOTHER_PHASE,
	IN_SAME_PHASE
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

	// For each node in incoming_links, whether it comes from another phase
	// or not. This is required because in some rather obscure cases,
	// it is possible to have an input twice in the same phase; both by
	// itself and as a bounced input.
	//
	// TODO: It is possible that we might even need to bounce multiple
	// times and thus disambiguate also between different external phases,
	// but we'll deal with that when we need to care about it, if ever.
	std::vector<NodeLinkType> incoming_link_type;

	// Used during the building of the effect chain.
	Colorspace output_color_space;
	GammaCurve output_gamma_curve;
	AlphaType output_alpha_type;
	Effect::MipmapRequirements needs_mipmaps;  // Directly or indirectly.

	// Set if this effect, and all effects consuming output from this node
	// (in the same phase) have one_to_one_sampling() set.
	bool one_to_one_sampling;

	// Same, for strong_one_to_one_sampling().
	bool strong_one_to_one_sampling;

	friend class EffectChain;
};

// A rendering phase; a single GLSL program rendering a single quad.
struct Phase {
	Node *output_node;

	GLuint glsl_program_num;  // Owned by the resource_pool.

	// Position and texcoord attribute indexes, although it doesn't matter
	// which is which, because they contain the same data.
	std::set<GLint> attribute_indexes;

	// Inputs are only inputs from other phases (ie., those that come from RTT);
	// input textures are counted as part of <effects>.
	std::vector<Phase *> inputs;
	// Bound sampler numbers for each input. Redundant in a sense
	// (it always corresponds to the index), but we need somewhere
	// to hold the value for the uniform.
	std::vector<int> input_samplers;
	std::vector<Node *> effects;  // In order.
	unsigned output_width, output_height, virtual_output_width, virtual_output_height;

	// Whether this phase is compiled as a compute shader, ie., the last effect is
	// marked as one.
	bool is_compute_shader;
	Node *compute_shader_node;

	// If <is_compute_shader>, which image unit the output buffer is bound to.
	// This is used as source for a Uniform<int> below.
	int outbuf_image_unit;

	// These are used in transforming from unnormalized to normalized coordinates
	// in compute shaders.
	int uniform_output_size[2];
	Point2D inv_output_size, output_texcoord_adjust;

	// Identifier used to create unique variables in GLSL.
	// Unique per-phase to increase cacheability of compiled shaders.
	std::map<std::pair<Node *, NodeLinkType>, std::string> effect_ids;

	// Uniforms for this phase; combined from all the effects.
	std::vector<Uniform<int>> uniforms_image2d;
	std::vector<Uniform<int>> uniforms_sampler2d;
	std::vector<Uniform<bool>> uniforms_bool;
	std::vector<Uniform<int>> uniforms_int;
	std::vector<Uniform<int>> uniforms_ivec2;
	std::vector<Uniform<float>> uniforms_float;
	std::vector<Uniform<float>> uniforms_vec2;
	std::vector<Uniform<float>> uniforms_vec3;
	std::vector<Uniform<float>> uniforms_vec4;
	std::vector<Uniform<Eigen::Matrix3d>> uniforms_mat3;

	// For measurement of GPU time used.
	std::list<GLuint> timer_query_objects_running;
	std::list<GLuint> timer_query_objects_free;
	uint64_t time_elapsed_ns;
	uint64_t num_measured_iterations;
};

class EffectChain {
public:
	// Aspect: e.g. 16.0f, 9.0f for 16:9.
	// resource_pool is a pointer to a ResourcePool with which to share shaders
	// and other resources (see resource_pool.h). If nullptr (the default),
	// will create its own that is not shared with anything else. Does not take
	// ownership of the passed-in ResourcePool, but will naturally take ownership
	// of its own internal one if created.
	EffectChain(float aspect_nom, float aspect_denom, ResourcePool *resource_pool = nullptr);
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
	Effect *add_effect(Effect *effect, Effect *input1, Effect *input2, Effect *input3, Effect *input4) {
		std::vector<Effect *> inputs;
		inputs.push_back(input1);
		inputs.push_back(input2);
		inputs.push_back(input3);
		inputs.push_back(input4);
		return add_effect(effect, inputs);
	}
	Effect *add_effect(Effect *effect, Effect *input1, Effect *input2, Effect *input3, Effect *input4, Effect *input5) {
		std::vector<Effect *> inputs;
		inputs.push_back(input1);
		inputs.push_back(input2);
		inputs.push_back(input3);
		inputs.push_back(input4);
		inputs.push_back(input5);
		return add_effect(effect, inputs);
	}
	Effect *add_effect(Effect *effect, const std::vector<Effect *> &inputs);

	// Adds an RGBA output. Note that you can have at most one RGBA output and two
	// Y'CbCr outputs (see below for details).
	void add_output(const ImageFormat &format, OutputAlphaFormat alpha_format);

	// Adds an YCbCr output. Note that you can only have at most two Y'CbCr
	// outputs, and they must have the same <ycbcr_format> and <type>.
	// (This limitation may be lifted in the future, to allow e.g. simultaneous
	// 8- and 10-bit output. Currently, multiple Y'CbCr outputs are only
	// useful in some very limited circumstances, like if one texture goes
	// to some place you cannot easily read from later.)
	//
	// Only 4:4:4 output is supported due to fragment shader limitations,
	// so chroma_subsampling_x and chroma_subsampling_y must both be 1.
	// <type> should match the data type of the FBO you are rendering to,
	// so that if you use 16-bit output (GL_UNSIGNED_SHORT), you will get
	// 8-, 10- or 12-bit output correctly as determined by <ycbcr_format.num_levels>.
	// Using e.g. ycbcr_format.num_levels == 1024 with GL_UNSIGNED_BYTE is
	// nonsensical and invokes undefined behavior.
	//
	// If you have both RGBA and Y'CbCr output(s), the RGBA output will come
	// in the last draw buffer. Also, <format> and <alpha_format> must be
	// identical between the two.
	void add_ycbcr_output(const ImageFormat &format, OutputAlphaFormat alpha_format,
	                      const YCbCrFormat &ycbcr_format,
			      YCbCrOutputSplitting output_splitting = YCBCR_OUTPUT_INTERLEAVED,
	                      GLenum output_type = GL_UNSIGNED_BYTE);

	// Change Y'CbCr output format. (This can be done also after finalize()).
	// Note that you are not allowed to change subsampling parameters;
	// however, you can change the color space parameters, ie.,
	// luma_coefficients, full_range and num_levels.
	void change_ycbcr_output_format(const YCbCrFormat &ycbcr_format);

	// Set number of output bits, to scale the dither.
	// 8 is the right value for most outputs.
	//
	// Special note for 10- and 12-bit Y'CbCr packed into GL_UNSIGNED_SHORT:
	// This is relative to the actual output, not the logical one, so you should
	// specify 16 here, not 10 or 12.
	//
	// The default, 0, is a special value that means no dither.
	void set_dither_bits(unsigned num_bits)
	{
		this->num_dither_bits = num_bits;
	}

	// Set where (0,0) is taken to be in the output. The default is
	// OUTPUT_ORIGIN_BOTTOM_LEFT, which is usually what you want
	// (see OutputOrigin above for more details).
	void set_output_origin(OutputOrigin output_origin)
	{
		this->output_origin = output_origin;
	}

	// Set intermediate format for framebuffers used when we need to bounce
	// to a temporary texture. The default, GL_RGBA16F, is good for most uses;
	// it is precise, has good range, and is relatively efficient. However,
	// if you need even more speed and your chain can do with some loss of
	// accuracy, you can change the format here (before calling finalize).
	// Calculations between bounce buffers are still in 32-bit floating-point
	// no matter what you specify.
	//
	// Of special interest is GL_SRGB8_ALPHA8, which stores sRGB-encoded RGB
	// and linear alpha; this is half the memory bandwidth of GL_RGBA16F,
	// while retaining reasonable precision for typical image data. It will,
	// however, cause some gamut clipping if your colorspace is far from sRGB,
	// as it cannot represent values outside [0,1]. NOTE: If you construct
	// a chain where you end up bouncing pixels in non-linear light
	// (gamma different from GAMMA_LINEAR), this will be the wrong thing.
	// However, it's hard to see how this could happen in a non-contrived
	// chain; few effects ever need texture bounce or resizing without also
	// combining multiple pixels, which really needs linear light and thus
	// triggers a conversion before the bounce.
	//
	// If you don't need alpha (or can do with very little of it), GL_RGB10_A2
	// is even better, as it has two more bits for each color component. There
	// is no GL_SRGB10, unfortunately, so on its own, it is somewhat worse than
	// GL_SRGB8, but you can set <transformation> to SQUARE_ROOT_FRAMEBUFFER_TRANSFORMATION,
	// and sqrt(x) will be stored instead of x. This is a rough approximation to
	// the sRGB curve, and reduces maximum error (in sRGB distance) by almost an
	// order of magnitude, well below what you can get from 8-bit true sRGB.
	// (Note that this strategy avoids the problem with bounced non-linear data
	// above, since the square root is turned off in that case.) However, texture
	// filtering will happen on the transformed values, so if you have heavy
	// downscaling or the likes (e.g. mipmaps), you could get subtly bad results.
	// You'll need to see which of the two that works the best for you in practice.
	void set_intermediate_format(
		GLenum intermediate_format,
		FramebufferTransformation transformation = NO_FRAMEBUFFER_TRANSFORMATION)
	{
		this->intermediate_format = intermediate_format;
		this->intermediate_transformation = transformation;
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

	// Note: If you already know the width and height of the viewport,
	// calling render_to_fbo() directly will be slightly more efficient,
	// as it saves it from getting it from OpenGL.
	void render_to_screen()
	{
		render_to_fbo(0, 0, 0);
	}

	// Render the effect chain to the given FBO. If width=height=0, keeps
	// the current viewport.
	void render_to_fbo(GLuint fbo, unsigned width, unsigned height);

	// Render the effect chain to the given set of textures. This is equivalent
	// to render_to_fbo() with a freshly created FBO bound to the given textures,
	// except that it is more efficient if the last phase contains a compute shader.
	// Thus, prefer this to render_to_fbo() where possible.
	//
	// Only one destination texture is supported. This restriction will be lifted
	// in the future.
	//
	// All destination textures must be exactly of size <width> x <height>,
	// and must either come from the same ResourcePool the effect uses, or outlive
	// the EffectChain (otherwise, we could be allocating FBOs that end up being
	// stale). Textures must also have valid state; in particular, they must either
	// be mipmap complete or have a non-mipmapped minification mode.
	//
	// width and height can not be zero.
	struct DestinationTexture {
		GLuint texnum;
		GLenum format;
	};
	void render_to_texture(const std::vector<DestinationTexture> &destinations, unsigned width, unsigned height);

	Effect *last_added_effect() {
		if (nodes.empty()) {
			return nullptr;
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

	// Whether input <input_num> of <node> corresponds to a single sampler
	// (see get_input_sampler()). Normally, you should not need to call this;
	// however, if the input Effect has set override_texture_bounce(),
	// this will return false, and you could be flexible and check it first
	// if you want.
	GLenum has_input_sampler(Node *node, unsigned input_num) const;

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

	// Do the actual rendering of the chain. If <dest_fbo> is not (GLuint)-1,
	// renders to that FBO. If <destinations> is non-empty, render to that set
	// of textures (last phase, save for the dummy phase, must be a compute shader),
	// with x/y ignored. Having both set is an error.
	void render(GLuint dest_fbo, const std::vector<DestinationTexture> &destinations,
	            unsigned x, unsigned y, unsigned width, unsigned height);

	// Execute one phase, ie. set up all inputs, effects and outputs, and render the quad.
	// If <destinations> is empty, uses whatever output is current (and the phase must not be
	// a compute shader).
	void execute_phase(Phase *phase,
	                   const std::map<Phase *, GLuint> &output_textures,
	                   const std::vector<DestinationTexture> &destinations,
	                   std::set<Phase *> *generated_mipmaps);

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
	void add_dummy_effect_if_needed();

	float aspect_nom, aspect_denom;
	ImageFormat output_format;
	OutputAlphaFormat output_alpha_format;

	bool output_color_rgba;
	int num_output_color_ycbcr;                      // Max 2.
	YCbCrFormat output_ycbcr_format;                 // If num_output_color_ycbcr is > 0.
	GLenum output_ycbcr_type;                        // If num_output_color_ycbcr is > 0.
	YCbCrOutputSplitting output_ycbcr_splitting[2];  // If num_output_color_ycbcr is > N.

	std::vector<Node *> nodes;
	std::map<Effect *, Node *> node_map;
	Effect *dither_effect;
	Node *ycbcr_conversion_effect_node;

	std::vector<Input *> inputs;  // Also contained in nodes.
	std::vector<Phase *> phases;

	GLenum intermediate_format;
	FramebufferTransformation intermediate_transformation;
	unsigned num_dither_bits;
	OutputOrigin output_origin;
	bool finalized;
	GLuint vbo;  // Contains vertex and texture coordinate data.

	// Whether the last effect (which will then be in a phase all by itself)
	// is a dummy effect that is only added because the last phase uses a compute
	// shader, which cannot output directly to the backbuffer. This means that
	// the phase can be skipped if we are _not_ rendering to the backbuffer.
	bool has_dummy_effect = false;

	ResourcePool *resource_pool;
	bool owns_resource_pool;

	bool do_phase_timing;
};

}  // namespace movit

#endif // !defined(_MOVIT_EFFECT_CHAIN_H)
