#ifndef _MOVIT_EFFECT_H
#define _MOVIT_EFFECT_H 1

// Effect is the base class for every effect. It basically represents a single
// GLSL function, with an optional set of user-settable parameters.
//
// A note on naming: Since all effects run in the same GLSL namespace,
// you can't use any name you want for global variables (e.g. uniforms).
// The framework assigns a prefix to you which will be unique for each
// effect instance; use the macro PREFIX() around your identifiers to
// automatically prepend that prefix.

#include <epoxy/gl.h>
#include <assert.h>
#include <stddef.h>
#include <map>
#include <string>
#include <vector>
#include <Eigen/Core>

#include "defs.h"

namespace movit {

class EffectChain;
class Node;

// Can alias on a float[2].
struct Point2D {
	Point2D() {}
	Point2D(float x, float y)
		: x(x), y(y) {}

	float x, y;
};

// Can alias on a float[3].
struct RGBTriplet {
	RGBTriplet() {}
	RGBTriplet(float r, float g, float b)
		: r(r), g(g), b(b) {}

	float r, g, b;
};

// Can alias on a float[4].
struct RGBATuple {
	RGBATuple() {}
	RGBATuple(float r, float g, float b, float a)
		: r(r), g(g), b(b), a(a) {}

	float r, g, b, a;
};

// Represents a registered uniform.
template<class T>
struct Uniform {
	std::string name;  // Without prefix.
	const T *value;  // Owner by the effect.
	size_t num_values;  // Number of elements; for arrays only. _Not_ the vector length.
	std::string prefix;  // Filled in only after phases have been constructed.
	GLint location;  // Filled in only after phases have been constructed. -1 if no location.
};

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
	// If you set INPUT_AND_OUTPUT_PREMULTIPLIED_ALPHA or
	// INPUT_PREMULTIPLIED_ALPHA_KEEP_BLANK, all of your inputs
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

		// Always outputs postmultiplied alpha. Only appropriate for inputs.
		OUTPUT_POSTMULTIPLIED_ALPHA,

		// Always outputs premultiplied alpha. As noted above,
		// you will then also get all inputs in premultiplied alpha.
		// If you set this, you should also set needs_linear_light().
		INPUT_AND_OUTPUT_PREMULTIPLIED_ALPHA,

		// Like INPUT_AND_OUTPUT_PREMULTIPLIED_ALPHA, but also guarantees
		// that if you get blank alpha in, you also keep blank alpha out.
		// This is a somewhat weaker guarantee than DONT_CARE_ALPHA_TYPE,
		// but is still useful in many situations, and appropriate when
		// e.g. you don't touch alpha at all.
		//
		// Does not make sense for inputs.
		INPUT_PREMULTIPLIED_ALPHA_KEEP_BLANK,

		// Keeps the type of alpha (premultiplied, postmultiplied, blank)
		// unchanged from input to output. Usually appropriate if you
		// process all color channels in a linear fashion, do not change
		// alpha, and do not produce any new pixels that have alpha != 1.0.
		//
		// Does not make sense for inputs.
		DONT_CARE_ALPHA_TYPE,
	};
	virtual AlphaHandling alpha_handling() const { return INPUT_AND_OUTPUT_PREMULTIPLIED_ALPHA; }

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

	// Whether this effect expects mipmaps or not.
	enum MipmapRequirements {
		// If chosen, you will be sampling with bilinear filtering,
		// ie. the closest mipmap will be chosen, and then there will be
		// bilinear interpolation inside it (GL_LINEAR_MIPMAP_NEAREST).
		NEEDS_MIPMAPS,

		// Whether the effect doesn't really care whether input textures
		// are with or without mipmaps. You could get the same effect
		// as NEEDS_MIPMAPS or CANNOT_ACCEPT_MIPMAPS; normally, you won't
		// get them, but if a different effect in the same phase needs mipmaps,
		// you will also get them.
		DOES_NOT_NEED_MIPMAPS,

		// The opposite of NEEDS_MIPMAPS; you will always be sampling from
		// the most detailed mip level (GL_LINEAR). Effects with NEEDS_MIPMAPS
		// and CANNOT_ACCEPT_MIPMAPS can not coexist within the same phase;
		// such phases will be split.
		//
		// This is the only choice that makes sense for a compute shader,
		// given that it doesn't have screen-space derivatives and thus
		// always will sample the most detailed mip level.
		CANNOT_ACCEPT_MIPMAPS,
	};
	virtual MipmapRequirements needs_mipmaps() const {
		if (is_compute_shader()) {
			return CANNOT_ACCEPT_MIPMAPS;
		} else {
			return DOES_NOT_NEED_MIPMAPS;
		}
	}

	// Whether there is a direct correspondence between input and output
	// texels. Specifically, the effect must not:
	//
	//   1. Try to sample in the border (ie., outside the 0.0 to 1.0 area).
	//   2. Try to sample between texels.
	//   3. Sample with an x- or y-derivative different from -1 or 1.
	//      (This also means needs_mipmaps() and one_to_one_sampling()
	//      together would make no sense.)
	//
	// The most common case for this would be an effect that has an exact
	// 1:1-correspondence between input and output texels, e.g. SaturationEffect.
	// However, more creative things, like mirroring/flipping or padding,
	// would also be allowed.
	//
	// The primary gain from setting this is that you can sample directly
	// from an effect that changes output size (see changes_output_size() below),
	// without going through a bounce texture. It won't work for effects that
	// set sets_virtual_output_size(), though.
	//
	// Does not make a lot of sense together with needs_texture_bounce().
	// Cannot be set for compute shaders.
	virtual bool one_to_one_sampling() const { return strong_one_to_one_sampling(); }

	// Similar in use to one_to_one_sampling(), but even stricter:
	// The effect must not modify texture coordinate in any way when
	// calling its input(s). This allows it to also be used after
	// a compute shader, in the same phase.
	//
	// An effect that it strong one-to-one must also be one-to-one.
	virtual bool strong_one_to_one_sampling() const { return false; }

	// Whether this effect wants to output to a different size than
	// its input(s) (see inform_input_size(), below). See also
	// sets_virtual_output_size() below.
	virtual bool changes_output_size() const { return false; }

	// Whether your get_output_size() function (see below) intends to ever set
	// virtual_width different from width, or similar for height.
	// It does not make sense to set this to true if changes_output_size() is false.
	virtual bool sets_virtual_output_size() const { return changes_output_size(); }

	// Whether this effect is effectively sampling from a a single texture.
	// If so, it will override needs_texture_bounce(); however, there are also
	// two demands it needs to fulfill:
	//
	//  1. It needs to be an Input, ie. num_inputs() == 0.
	//  2. It needs to allocate exactly one sampler in set_gl_state(),
	//     and allow dependent effects to change that sampler state.
	virtual bool is_single_texture() const { return false; }

	// If set, this effect should never be bounced to an output, even if a
	// dependent effect demands texture bounce.
	//
	// Note that setting this can invoke undefined behavior, up to and including crashing,
	// so you should only use it if you have deep understanding of your entire chain
	// and Movit's processing of it. The most likely use case is if you have an input
	// that's cheap to compute but not a single texture (e.g. YCbCrInput), and want
	// to run a ResampleEffect directly from it. Normally, this would require a bounce,
	// but it's faster not to. (However, also note that in this case, effective texel
	// subpixel precision will be too optimistic, since chroma is already subsampled.)
	//
	// Has no effect if is_single_texture() is set.
	virtual bool override_disable_bounce() const { return false; }

	// If changes_output_size() is true, you must implement this to tell
	// the framework what output size you want. Also, you can set a
	// virtual width/height, which is the size the next effect (if any)
	// will _think_ your data is in. This is primarily useful if you are
	// relying on getting OpenGL's bilinear resizing for free; otherwise,
	// your virtual_width/virtual_height should be the same as width/height.
	//
	// Note that it is explicitly allowed to change width and height
	// from frame to frame; EffectChain will reallocate textures as needed.
	virtual void get_output_size(unsigned *width, unsigned *height,
	                             unsigned *virtual_width, unsigned *virtual_height) const {
		assert(false);
	}

	// Whether this effect uses a compute shader instead of a regular fragment shader.
	// Compute shaders are more flexible in that they can have multiple outputs
	// for each invocation and also communicate between instances (by using shared
	// memory within each group), but are not universally supported. The typical
	// pattern would be to check movit_compute_shaders_supported and rewrite the
	// graph to use a compute shader effect instead of a regular effect if it is
	// available, in order to get better performance. Since compute shaders can reuse
	// loads (again typically through shared memory), using needs_texture_bounce()
	// is usually not needed, although it is allowed; the best candidates for compute
	// shaders are typically those that sample many times from their input
	// but can reuse those loads across neighboring instances.
	//
	// Compute shaders commonly work with unnormalized texture coordinates
	// (where coordinates are integers [0..W) and [0..H)), whereas the rest
	// of Movit, including any inputs you may want to sample from, works
	// with normalized coordinates ([0..1)). Movit gives you uniforms
	// PREFIX(inv_output_size) and PREFIX(output_texcoord_adjust) that you
	// can use to transform unnormalized to normalized, as well as a macro
	// NORMALIZE_TEXTURE_COORDS(vec2) that does it for you.
	//
	// Since compute shaders have flexible output, it is difficult to chain other
	// effects after them in the same phase, and thus, they will always be last.
	// (This limitation may be lifted for the special case of one-to-one effects
	// in the future.) Furthermore, they cannot write to the framebuffer, just to
	// textures, so Movit may have to insert an extra phase just to do the output
	// from a texture to the screen in some cases. However, this is transparent
	// to both the effect and the user.
	virtual bool is_compute_shader() const { return false; }

	// For a compute shader (see the previous member function), what dimensions
	// it should be invoked over. Called every frame, before uniforms are set
	// (so you are allowed to update uniforms based from this call).
	virtual void get_compute_dimensions(unsigned output_width, unsigned output_height,
	                                    unsigned *x, unsigned *y, unsigned *z) const {
		*x = output_width;
		*y = output_height;
		*z = 1;
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

	// Inform the effect that it has been just added to the EffectChain.
	// The primary use for this is to store the ResourcePool uesd by
	// the chain; for modifications to it, rewrite_graph() below
	// is probably a better fit.
	virtual void inform_added(EffectChain *chain) {}

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
	virtual bool set_int(const std::string &key, int value) MUST_CHECK_RESULT;
	virtual bool set_ivec2(const std::string &key, const int *values) MUST_CHECK_RESULT;
	virtual bool set_float(const std::string &key, float value) MUST_CHECK_RESULT;
	virtual bool set_vec2(const std::string &key, const float *values) MUST_CHECK_RESULT;
	virtual bool set_vec3(const std::string &key, const float *values) MUST_CHECK_RESULT;
	virtual bool set_vec4(const std::string &key, const float *values) MUST_CHECK_RESULT;

protected:
	// Register a parameter. Whenever set_*() is called with the same key,
	// it will update the value in the given pointer (typically a pointer
	// to some private member variable in your effect). It will also
	// register a uniform of the same name (plus an arbitrary prefix
	// which you can access using the PREFIX macro) that you can access.
	//
	// Neither of these take ownership of the pointer.

	// These correspond directly to int/float/vec2/vec3/vec4 in GLSL.
	void register_int(const std::string &key, int *value);
	void register_ivec2(const std::string &key, int *values);
	void register_float(const std::string &key, float *value);
	void register_vec2(const std::string &key, float *values);
	void register_vec3(const std::string &key, float *values);
	void register_vec4(const std::string &key, float *values);

	// Register uniforms, such that they will automatically be set
	// before the shader runs. This is more efficient than set_uniform_*
	// in effect_util.h, because it doesn't need to do name lookups
	// every time. Also, in the future, it will use uniform buffer objects
	// (UBOs) if available to reduce the number of calls into the driver.
	//
	// May not be called after output_fragment_shader() has returned.
	// The pointer must be valid for the entire lifetime of the Effect,
	// since the value is pulled from it each execution. The value is
	// guaranteed to be read after set_gl_state() for the effect has
	// returned, so you can safely update its value from there.
	//
	// Note that this will also declare the uniform in the shader for you,
	// so you should not do that yourself. (This is so it can be part of
	// the right uniform block.) However, it is probably a good idea to
	// have a commented-out declaration so that it is easier to see the
	// type and thus understand the shader on its own.
	//
	// Calling register_* will automatically imply register_uniform_*,
	// except for register_int as noted above.
	void register_uniform_sampler2d(const std::string &key, const int *value);
	void register_uniform_bool(const std::string &key, const bool *value);
	void register_uniform_int(const std::string &key, const int *value);
	void register_uniform_ivec2(const std::string &key, const int *values);
	void register_uniform_float(const std::string &key, const float *value);
	void register_uniform_vec2(const std::string &key, const float *values);
	void register_uniform_vec3(const std::string &key, const float *values);
	void register_uniform_vec4(const std::string &key, const float *values);
	void register_uniform_float_array(const std::string &key, const float *values, size_t num_values);
	void register_uniform_vec2_array(const std::string &key, const float *values, size_t num_values);
	void register_uniform_vec3_array(const std::string &key, const float *values, size_t num_values);
	void register_uniform_vec4_array(const std::string &key, const float *values, size_t num_values);
	void register_uniform_mat3(const std::string &key, const Eigen::Matrix3d *matrix);

private:
	std::map<std::string, int *> params_int;
	std::map<std::string, int *> params_ivec2;
	std::map<std::string, float *> params_float;
	std::map<std::string, float *> params_vec2;
	std::map<std::string, float *> params_vec3;
	std::map<std::string, float *> params_vec4;

	// Picked out by EffectChain during finalization.
	std::vector<Uniform<int>> uniforms_image2d;
	std::vector<Uniform<int>> uniforms_sampler2d;
	std::vector<Uniform<bool>> uniforms_bool;
	std::vector<Uniform<int>> uniforms_int;
	std::vector<Uniform<int>> uniforms_ivec2;
	std::vector<Uniform<float>> uniforms_float;
	std::vector<Uniform<float>> uniforms_vec2;
	std::vector<Uniform<float>> uniforms_vec3;
	std::vector<Uniform<float>> uniforms_vec4;
	std::vector<Uniform<float>> uniforms_float_array;
	std::vector<Uniform<float>> uniforms_vec2_array;
	std::vector<Uniform<float>> uniforms_vec3_array;
	std::vector<Uniform<float>> uniforms_vec4_array;
	std::vector<Uniform<Eigen::Matrix3d>> uniforms_mat3;
	friend class EffectChain;
};

}  // namespace movit

#endif // !defined(_MOVIT_EFFECT_H)
