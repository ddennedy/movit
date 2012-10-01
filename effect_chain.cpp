#define GL_GLEXT_PROTOTYPES 1

#include <stdio.h>
#include <assert.h>

#include <GL/gl.h>
#include <GL/glext.h>

#include "util.h"
#include "effect_chain.h"
#include "gamma_expansion_effect.h"
#include "lift_gamma_gain_effect.h"
#include "colorspace_conversion_effect.h"

EffectChain::EffectChain(unsigned width, unsigned height)
	: width(width), height(height), finalized(false) {}

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

	// not handled yet
	assert(!effect->needs_many_samples());
	assert(!effect->needs_mipmaps());

	effects.push_back(effect);
	return effect;
}

void EffectChain::finalize()
{
	std::string frag_shader = read_file("header.glsl");

	for (unsigned i = 0; i < effects.size(); ++i) {
		char effect_id[256];
		sprintf(effect_id, "eff%d", i);
	
		frag_shader += "\n";
		frag_shader += std::string("#define PREFIX(x) ") + effect_id + "_ ## x\n";
		frag_shader += std::string("#define FUNCNAME ") + effect_id + "\n";
		frag_shader += effects[i]->output_glsl();
		frag_shader += "#undef PREFIX\n";
		frag_shader += "#undef FUNCNAME\n";
		frag_shader += std::string("#define LAST_INPUT ") + effect_id + "\n";
		frag_shader += "\n";
	}
	printf("%s\n", frag_shader.c_str());
	
	glsl_program_num = glCreateProgram();
	GLhandleARB vs_obj = compile_shader(read_file("vs.glsl"), GL_VERTEX_SHADER);
	GLhandleARB fs_obj = compile_shader(frag_shader, GL_FRAGMENT_SHADER);
	glAttachObjectARB(glsl_program_num, vs_obj);
	check_error();
	glAttachObjectARB(glsl_program_num, fs_obj);
	check_error();
	glLinkProgram(glsl_program_num);
	check_error();
}
