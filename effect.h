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

#include <GL/gl.h>

class EffectChain;

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
void set_uniform_float_array(GLuint glsl_program_num, const std::string &prefix, const std::string &key, const float *values, size_t num_values);
void set_uniform_vec2(GLuint glsl_program_num, const std::string &prefix, const std::string &key, const float *values);
void set_uniform_vec3(GLuint glsl_program_num, const std::string &prefix, const std::string &key, const float *values);
void set_uniform_vec4_array(GLuint glsl_program_num, const std::string &prefix, const std::string &key, const float *values, size_t num_values);

class Effect {
public: 
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
	//     you sample on whole input pixels only, it makes no difference.)
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

	// How many inputs this effect will take (a fixed number).
	// If you have only one input, it will be called INPUT() in GLSL;
	// if you have several, they will be INPUT1(), INPUT2(), and so on.
	virtual unsigned num_inputs() const { return 1; }

	// Requests that this effect adds itself to the given effect chain.
	// For most effects, the default will be fine, but for effects that
	// consist of multiple passes, it is often useful to replace this
	// with something that adds completely different things to the chain.
	virtual void add_self_to_effect_chain(EffectChain *graph, const std::vector<Effect *> &inputs);

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
	virtual bool set_int(const std::string&, int value);
	virtual bool set_float(const std::string &key, float value);
	virtual bool set_vec2(const std::string &key, const float *values);
	virtual bool set_vec3(const std::string &key, const float *values);

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
