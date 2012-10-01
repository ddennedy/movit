#include <assert.h>

#include "effect_chain.h"
#include "gamma_expansion_effect.h"
#include "lift_gamma_gain_effect.h"
#include "colorspace_conversion_effect.h"

EffectChain::EffectChain(unsigned width, unsigned height)
	: width(width), height(height) {}

void EffectChain::add_input(const ImageFormat &format)
{
	input_format = format;
	current_color_space = format.color_space;
	current_gamma_curve = format.gamma_curve;
}

void EffectChain::add_output(const ImageFormat &format)
{
	output_format = format;
}
	
Effect *instantiate_effect(EffectId effect)
{
	switch (effect) {
	case GAMMA_CONVERSION:
		return new GammaExpansionEffect();
	case RGB_PRIMARIES_CONVERSION:
		return new GammaExpansionEffect();
	case LIFT_GAMMA_GAIN:
		return new LiftGammaGainEffect();
	}
	assert(false);
}

Effect *EffectChain::add_effect(EffectId effect_id)
{
	Effect *effect = instantiate_effect(effect_id);

	if (effect->needs_linear_light() && current_gamma_curve != GAMMA_LINEAR) {
		GammaExpansionEffect *gamma_conversion = new GammaExpansionEffect();
		gamma_conversion->set_int("source_curve", current_gamma_curve);
		effects.push_back(gamma_conversion);
		current_gamma_curve = GAMMA_LINEAR;
	}

	if (effect->needs_srgb_primaries() && current_color_space != COLORSPACE_sRGB) {
		assert(current_gamma_curve == GAMMA_LINEAR);
		ColorSpaceConversionEffect *colorspace_conversion = new ColorSpaceConversionEffect();
		colorspace_conversion->set_int("source_space", current_color_space);
		colorspace_conversion->set_int("destination_space", COLORSPACE_sRGB);
		effects.push_back(colorspace_conversion);
		current_color_space = COLORSPACE_sRGB;
	}

	effects.push_back(effect);
	return effect;
}

