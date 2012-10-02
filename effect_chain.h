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

	// input, effects, output, finalize need to come in that specific order.

	void add_input(const ImageFormat &format);

	// The returned pointer is owned by EffectChain.
	Effect *add_effect(EffectId effect);

	void add_output(const ImageFormat &format);
	void finalize();

	//void render(unsigned char *src, unsigned char *dst);
	void render_to_screen(unsigned char *src);

private:
	void normalize_to_linear_gamma();
	void normalize_to_srgb();

	unsigned width, height;
	ImageFormat input_format, output_format;
	std::vector<Effect *> effects;

	bool use_srgb_texture_format;

	int glsl_program_num;
	bool finalized;

	// Used during the building of the effect chain.
	ColorSpace current_color_space;
	GammaCurve current_gamma_curve;	
};


#endif // !defined(_EFFECT_CHAIN_H)
