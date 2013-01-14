#ifndef _EFFECT_H
#define _EFFECT_H 1

// Effect is the base class for every effect. It basically represents a single
// GLSL function, with an optional set of user-settable parameters.
//
// A note on naming: Since all effects run in the same GLSL namespace,
// you can't use any name you want for global variables (e.g. uniforms).
// The framework assigns a prefix to you which will be unique for each
// effect instance; use the macro PREFIX() around your identifiers to
// automatically prepend that prefix.

#include <map>
#include <string>
#include <vector>

#include <assert.h>

#include <Eigen/Core>

#include <GL/glew.h>
#include "util.h"

class EffectChain;
class Node;

// Can alias on a float[2].
struct Point2D {
	Point2D(float x, float y)
		: x(x), y(y) {}

	float x, y;
};

// Can alias on a float[3].
struct RGBTriplet {
	RGBTriplet(float r, float g, float b)
		: r(r), g(g), b(b) {}

	float r, g, b;
};

// Convenience functions that deal with prepending the prefix.
GLint get_uniform_location(GLuint glsl_program_num, const std::string &prefix, const std::string &key);
void set_uniform_int(GLuint glsl_program_num, const std::string &prefix, const std::string &key, int value);
void set_uniform_float(GLuint glsl_program_num, const std::string &prefix, const std::string &key, float value);
void set_uniform_vec2(GLuint glsl_program_num, const std::string &prefix, const std::string &key, const float *values);
void set_uniform_vec3(GLuint glsl_program_num, const std::string &prefix, const std::string &key, const float *values);
void set_uniform_vec4_array(GLuint glsl_program_num, const std::string &prefix, const std::string &key, const float *values, size_t num_values);
void set_uniform_mat3(GLuint glsl_program_num, const std::string &prefix, const std::string &key, const Eigen::Matrix3d &matrix);

class Effect {
public:
	virtual ~Effect() {}

	// An identifier for this type of effect, mostly used for debug output
	// (but some special names, like "ColorspaceConversionEffect", holds special
	// meaning). Same as the class name is fine.
	virtual std::string effect_type_id() const = 0;

	// Whether this effects expects its input (and output) to be in
	// linear gamma, ie. without an applied gamma curve. Most effects
	// will want this, although the ones that never actually look at
	// the pixels, e.g. mirror, won't need to care, and can set this
	// to false. If so, the input gamma will be undefined.
	//
	// Also see the note on needs_texture_bounce(), below.
	virtual bool needs_linear_light() const { return true; }

	// Whether this effect expects its input to be in the sRGB
	// color space, ie. use the sRGB/Rec. 709 RGB primaries.
	// (If not, it would typically come in as some slightly different
	// set of RGB primaries; you would currently not get YCbCr
	// or something similar).
	//
	// Again, most effects will want this, but you can set it to false
	// if you process each channel independently, equally _and_
	// in a linear fashion.
	virtual bool needs_srgb_primaries() const { return true; }

	// How this effect handles alpha, ie. what it outputs in its
	// alpha channel. The choices are basically blank (alpha is always 1.0),
	// premultiplied and postmultiplied.
	//
	// Premultiplied alpha is when the alpha value has been be multiplied
	// into the three color components, so e.g. 100% red at 50% alpha
	// would be (0.5, 0.0, 0.0, 0.5) instead of (1.0, 0.0, 0.0, 0.5)
	// as it is stored in most image formats (postmultiplied alpha).
	// The multiplication is taken to have happened in linear light.
	// This is the most natural format for processing, and the default in
	// most of Movit (just like linear light is).
	//
	// If you set INPUT_AND_OUTPUT_ALPHA_PREMULTIPLIED, all of your inputs
	// (if any) are guaranteed to also be in premultiplied alpha.
	// Otherwise, you can get postmultiplied or premultiplied alpha;
	// you won't know. If you have multiple inputs, you will get the same
	// (pre- or postmultiplied) for all inputs, although most likely,
	// you will want to combine them in a premultiplied fashion anyway
	// in that case.
	enum AlphaHandling {
		// Always outputs blank alpha (ie. alpha=1.0). Only appropriate
		// for inputs that do not output an alpha channel.
		// Blank alpha is special in that it can be treated as both
		// pre- and postmultiplied.
		OUTPUT_BLANK_ALPHA,

		// Always outputs premultiplied alpha. As noted above,
		// you will then also get all inputs in premultiplied alpha.
		// If you set this, you should also set needs_linear_light().
		INPUT_AND_OUTPUT_ALPHA_PREMULTIPLIED,

		// Always outputs postmultiplied alpha. Only appropriate for inputs.
		OUTPUT_ALPHA_POSTMULTIPLIED,

		// Keeps the type of alpha unchanged from input to output.
		// Usually appropriate if you process all color channels
		// in a linear fashion, and do not change alpha.
		//
		// Does not make sense for inputs.
		DONT_CARE_ALPHA_TYPE,
	};
	virtual AlphaHandling alpha_handling() const { return INPUT_AND_OUTPUT_ALPHA_PREMULTIPLIED; }

	// Whether this effect expects its input to come directly from
	// a texture. If this is true, the framework will not chain the
	// input from other effects, but will store the results of the
	// chain to a temporary (RGBA fp16) texture and let this effect
	// sample directly from that.
	//
	// There are two good reasons why you might want to set this:
	//
	//  1. You are sampling more than once from the input,
	//     in which case computing all the previous steps might
	//     be more expensive than going to a memory intermediate.
	//  2. You rely on previous effects, possibly including gamma
	//     expansion, to happen pre-filtering instead of post-filtering.
	//     (This is only relevant if you actually need the filtering; if
	//     you sample 1:1 between pixels and texels, it makes no difference.)
	//
	// Note that in some cases, you might get post-filtered gamma expansion
	// even when setting this option. More specifically, if you are the
	// first effect in the chain, and the GPU is doing sRGB gamma
	// expansion, it is undefined (from OpenGL's side) whether expansion
	// happens pre- or post-filtering. For most uses, however,
	// either will be fine.
	virtual bool needs_texture_bounce() const { return false; }

	// Whether this effect expects mipmaps or not. If you set this to
	// true, you will be sampling with bilinear filtering; if not,
	// you could be sampling with simple linear filtering and no mipmaps
	// (although there is no guarantee; if a different effect in the chain
	// needs mipmaps, you will also get them).
	virtual bool needs_mipmaps() const { return false; }

	// Whether this effect wants to output to a different size than
	// its input(s) (see inform_input_size(), below). If you set this to
	// true, the output will be bounced to a texture (similarly to if the
	// next effect set needs_texture_bounce()).
	virtual bool changes_output_size() const { return false; }

	// If changes_output_size() is true, you must implement this to tell
	// the framework what output size you want.
	//
	// Note that it is explicitly allowed to change width and height
	// from frame to frame; EffectChain will reallocate textures as needed.
	virtual void get_output_size(unsigned *width, unsigned *height) const {
		assert(false);
	}

	// Tells the effect the resolution of each of its input.
	// This will be called every frame, and always before get_output_size(),
	// so you can change your output size based on the input if so desired.
	//
	// Note that in some cases, an input might not have a single well-defined
	// resolution (for instance if you fade between two inputs with
	// different resolutions). In this case, you will get width=0 and height=0
	// for that input. If you cannot handle that, you will need to set
	// needs_texture_bounce() to true, which will force a render to a single
	// given resolution before you get the input.
	virtual void inform_input_size(unsigned input_num, unsigned width, unsigned height) {}

	// How many inputs this effect will take (a fixed number).
	// If you have only one input, it will be called INPUT() in GLSL;
	// if you have several, they will be INPUT1(), INPUT2(), and so on.
	virtual unsigned num_inputs() const { return 1; }

	// Let the effect rewrite the effect chain as it sees fit.
	// Most effects won't need to do this, but this is very useful
	// if you have an effect that consists of multiple sub-effects
	// (for instance, two passes). The effect is given to its own
	// pointer, and it can add new ones (by using add_node()
	// and connect_node()) as it sees fit. This is called at
	// EffectChain::finalize() time, when the entire graph is known,
	// in the order that the effects were originally added.
	//
	// Note that if the effect wants to take itself entirely out
	// of the chain, it must set “disabled” to true and then disconnect
	// itself from all other effects.
	virtual void rewrite_graph(EffectChain *graph, Node *self) {}

	// Outputs one GLSL uniform declaration for each registered parameter
	// (see below), with the right prefix prepended to each uniform name.
	// If you do not want this behavior, you can override this function.
	virtual std::string output_convenience_uniforms() const;

	// Returns the GLSL fragment shader string for this effect.
	virtual std::string output_fragment_shader() = 0;

	// Set all OpenGL state that this effect needs before rendering.
	// The default implementation sets one uniform per registered parameter,
	// but no other state.
	//
	// <sampler_num> is the first free texture sampler. If you want to use
	// textures, you can bind a texture to GL_TEXTURE0 + <sampler_num>,
	// and then increment the number (so that the next effect in the chain
	// will use a different sampler).
	virtual void set_gl_state(GLuint glsl_program_num, const std::string& prefix, unsigned *sampler_num);

	// If you set any special OpenGL state in set_gl_state(), you can clear it
	// after rendering here. The default implementation does nothing.
	virtual void clear_gl_state();

	// Set a parameter; intended to be called from user code.
	// Neither of these take ownership of the pointer.
	virtual bool set_int(const std::string&, int value) MUST_CHECK_RESULT;
	virtual bool set_float(const std::string &key, float value) MUST_CHECK_RESULT;
	virtual bool set_vec2(const std::string &key, const float *values) MUST_CHECK_RESULT;
	virtual bool set_vec3(const std::string &key, const float *values) MUST_CHECK_RESULT;

protected:
	// Register a parameter. Whenever set_*() is called with the same key,
	// it will update the value in the given pointer (typically a pointer
	// to some private member variable in your effect).
	//
	// Neither of these take ownership of the pointer.

	// int is special since GLSL pre-1.30 doesn't have integer uniforms.
	// Thus, ints that you register will _not_ be converted to GLSL uniforms.
	void register_int(const std::string &key, int *value);

	// These correspond directly to float/vec2/vec3 in GLSL.
	void register_float(const std::string &key, float *value);
	void register_vec2(const std::string &key, float *values);
	void register_vec3(const std::string &key, float *values);

	// This will register a 1D texture, which will be bound to a sampler
	// when your GLSL code runs (so it corresponds 1:1 to a sampler2D uniform
	// in GLSL).
	//
	// Note that if you change the contents of <values>, you will need to
	// call invalidate_1d_texture() to have the picture re-uploaded on the
	// next frame. This is in contrast to all the other parameters, which are
	// set anew every frame.
	void register_1d_texture(const std::string &key, float *values, size_t size);
	void invalidate_1d_texture(const std::string &key);
	
private:
	struct Texture1D {
		float *values;
		size_t size;
		bool needs_update;
		GLuint texture_num;
	};

	std::map<std::string, int *> params_int;
	std::map<std::string, float *> params_float;
	std::map<std::string, float *> params_vec2;
	std::map<std::string, float *> params_vec3;
	std::map<std::string, Texture1D> params_tex_1d;
};

#endif // !defined(_EFFECT_H)
