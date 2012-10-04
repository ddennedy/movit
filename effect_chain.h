#ifndef _EFFECT_CHAIN_H
#define _EFFECT_CHAIN_H 1

#include <vector>

#include "effect.h"
#include "effect_id.h"

enum PixelFormat { FORMAT_RGB, FORMAT_RGBA, FORMAT_BGR, FORMAT_BGRA };

enum ColorSpace {
	COLORSPACE_sRGB = 0,
	COLORSPACE_REC_709 = 0,  // Same as sRGB.
	COLORSPACE_REC_601_525 = 1,
	COLORSPACE_REC_601_625 = 2,
};

enum GammaCurve {
	GAMMA_LINEAR = 0,
	GAMMA_sRGB = 1,
	GAMMA_REC_601 = 2,
	GAMMA_REC_709 = 2,  // Same as Rec. 601.
};

struct ImageFormat {
	PixelFormat pixel_format;
	ColorSpace color_space;
	GammaCurve gamma_curve;
};

class EffectChain {
public:
	EffectChain(unsigned width, unsigned height);

	// User API:
	// input, effects, output, finalize need to come in that specific order.

	void add_input(const ImageFormat &format);

	// The returned pointer is owned by EffectChain.
	Effect *add_effect(EffectId effect) {
		return add_effect(effect, get_last_added_effect());
	}
	Effect *add_effect(EffectId effect, Effect *input) {
		std::vector<Effect *> inputs;
		inputs.push_back(input);
		return add_effect(effect, inputs);
	}
	Effect *add_effect(EffectId effect, Effect *input1, Effect *input2) {
		std::vector<Effect *> inputs;
		inputs.push_back(input1);
		inputs.push_back(input2);
		return add_effect(effect, inputs);
	}
	Effect *add_effect(EffectId effect, const std::vector<Effect *> &inputs);

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
	void render_to_screen(unsigned char *src);

	Effect *get_last_added_effect() { 
		return last_added_effect;
	}

private:
	struct Phase {
		GLint glsl_program_num;
		bool input_needs_mipmaps;
		unsigned start, end;
	};

	Effect *normalize_to_linear_gamma(Effect *input);
	Effect *normalize_to_srgb(Effect *input);

	// Create a GLSL program computing effects [start, end>.
	Phase compile_glsl_program(unsigned start_index, unsigned end_index);

	unsigned width, height;
	ImageFormat input_format, output_format;
	std::vector<Effect *> effects;
	std::multimap<Effect *, Effect *> outgoing_links;
	std::multimap<Effect *, Effect *> incoming_links;
	Effect *last_added_effect;

	GLuint source_image_num;
	bool use_srgb_texture_format;

	GLuint fbo;
	GLuint temp_textures[2];

	std::vector<Phase> phases;

	GLenum format, bytes_per_pixel;
	bool finalized;

	// Used during the building of the effect chain.
	std::map<Effect *, ColorSpace> output_color_space;
	std::map<Effect *, GammaCurve> output_gamma_curve;	
};


#endif // !defined(_EFFECT_CHAIN_H)
