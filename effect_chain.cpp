#define GL_GLEXT_PROTOTYPES 1

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <GL/gl.h>
#include <GL/glext.h>

#include "util.h"
#include "effect_chain.h"
#include "gamma_expansion_effect.h"
#include "gamma_compression_effect.h"
#include "lift_gamma_gain_effect.h"
#include "colorspace_conversion_effect.h"
#include "saturation_effect.h"
#include "vignette_effect.h"
#include "texture_enum.h"

EffectChain::EffectChain(unsigned width, unsigned height)
	: width(width), height(height), use_srgb_texture_format(false), finalized(false) {}

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
	case EFFECT_GAMMA_EXPANSION:
		return new GammaExpansionEffect();
	case EFFECT_GAMMA_COMPRESSION:
		return new GammaCompressionEffect();
	case EFFECT_COLOR_SPACE_CONVERSION:
		return new ColorSpaceConversionEffect();
	case EFFECT_LIFT_GAMMA_GAIN:
		return new LiftGammaGainEffect();
	case EFFECT_SATURATION:
		return new SaturationEffect();
	case EFFECT_VIGNETTE:
		return new VignetteEffect();
	}
	assert(false);
}

void EffectChain::normalize_to_linear_gamma()
{
	if (current_gamma_curve == GAMMA_sRGB) {
		// TODO: check if the extension exists
		use_srgb_texture_format = true;
	} else {
		GammaExpansionEffect *gamma_conversion = new GammaExpansionEffect();
		gamma_conversion->set_int("source_curve", current_gamma_curve);
		effects.push_back(gamma_conversion);
	}
	current_gamma_curve = GAMMA_LINEAR;
}

void EffectChain::normalize_to_srgb()
{
	assert(current_gamma_curve == GAMMA_LINEAR);
	ColorSpaceConversionEffect *colorspace_conversion = new ColorSpaceConversionEffect();
	colorspace_conversion->set_int("source_space", current_color_space);
	colorspace_conversion->set_int("destination_space", COLORSPACE_sRGB);
	effects.push_back(colorspace_conversion);
	current_color_space = COLORSPACE_sRGB;
}

Effect *EffectChain::add_effect(EffectId effect_id)
{
	Effect *effect = instantiate_effect(effect_id);

	if (effect->needs_linear_light() && current_gamma_curve != GAMMA_LINEAR) {
		normalize_to_linear_gamma();
	}

	if (effect->needs_srgb_primaries() && current_color_space != COLORSPACE_sRGB) {
		normalize_to_srgb();
	}

	// not handled yet
	assert(!effect->needs_many_samples());
	assert(!effect->needs_mipmaps());

	effects.push_back(effect);
	return effect;
}

// GLSL pre-1.30 doesn't support token pasting. Replace PREFIX(x) with <effect_id>_x.
std::string replace_prefix(const std::string &text, const std::string &prefix)
{
	std::string output;
	size_t start = 0;

	while (start < text.size()) {
		size_t pos = text.find("PREFIX(", start);
		if (pos == std::string::npos) {
			output.append(text.substr(start, std::string::npos));
			break;
		}

		output.append(text.substr(start, pos - start));
		output.append(prefix);
		output.append("_");

		pos += strlen("PREFIX(");
	
		// Output stuff until we find the matching ), which we then eat.
		int depth = 1;
		size_t end_arg_pos = pos;
		while (end_arg_pos < text.size()) {
			if (text[end_arg_pos] == '(') {
				++depth;
			} else if (text[end_arg_pos] == ')') {
				--depth;
				if (depth == 0) {
					break;
				}
			}
			++end_arg_pos;
		}
		output.append(text.substr(pos, end_arg_pos - pos));
		++end_arg_pos;
		assert(depth == 0);
		start = end_arg_pos;
	}
	return output;
}

void EffectChain::finalize()
{
	if (current_color_space != output_format.color_space) {
		ColorSpaceConversionEffect *colorspace_conversion = new ColorSpaceConversionEffect();
		colorspace_conversion->set_int("source_space", current_color_space);
		colorspace_conversion->set_int("destination_space", output_format.color_space);
		effects.push_back(colorspace_conversion);
		current_color_space = output_format.color_space;
	}

	if (current_gamma_curve != output_format.gamma_curve) {
		if (current_gamma_curve != GAMMA_LINEAR) {
			normalize_to_linear_gamma();
		}
		assert(current_gamma_curve == GAMMA_LINEAR);
		GammaCompressionEffect *gamma_conversion = new GammaCompressionEffect();
		gamma_conversion->set_int("destination_curve", output_format.gamma_curve);
		effects.push_back(gamma_conversion);
		current_gamma_curve = output_format.gamma_curve;
	}

	std::string frag_shader = read_file("header.glsl");

	for (unsigned i = 0; i < effects.size(); ++i) {
		char effect_id[256];
		sprintf(effect_id, "eff%d", i);
	
		frag_shader += "\n";
		frag_shader += std::string("#define FUNCNAME ") + effect_id + "\n";
		frag_shader += replace_prefix(effects[i]->output_convenience_uniforms(), effect_id);
		frag_shader += replace_prefix(effects[i]->output_glsl(), effect_id);
		frag_shader += "#undef PREFIX\n";
		frag_shader += "#undef FUNCNAME\n";
		frag_shader += "#undef LAST_INPUT\n";
		frag_shader += std::string("#define LAST_INPUT ") + effect_id + "\n";
		frag_shader += "\n";
	}
	frag_shader.append(read_file("footer.glsl"));
	printf("%s\n", frag_shader.c_str());
	
	glsl_program_num = glCreateProgram();
	GLuint vs_obj = compile_shader(read_file("vs.glsl"), GL_VERTEX_SHADER);
	GLuint fs_obj = compile_shader(frag_shader, GL_FRAGMENT_SHADER);
	glAttachShader(glsl_program_num, vs_obj);
	check_error();
	glAttachShader(glsl_program_num, fs_obj);
	check_error();
	glLinkProgram(glsl_program_num);
	check_error();

	finalized = true;
}

void EffectChain::render_to_screen(unsigned char *src)
{
	assert(finalized);

	check_error();
	glUseProgram(glsl_program_num);
	check_error();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, SOURCE_IMAGE);

	GLenum format, internal_format;
	if (use_srgb_texture_format) {
		internal_format = GL_SRGB8;
	} else {
		internal_format = GL_RGBA8;
	}
	if (input_format.pixel_format == FORMAT_RGB) {
		format = GL_RGB;
	} else if (input_format.pixel_format == FORMAT_RGBA) {
		format = GL_RGBA;
	} else {
		assert(false);
	}

	glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, GL_UNSIGNED_BYTE, src);
	check_error();
	glUniform1i(glGetUniformLocation(glsl_program_num, "input_tex"), 0);

	for (unsigned i = 0; i < effects.size(); ++i) {
		char effect_id[256];
		sprintf(effect_id, "eff%d", i);
		effects[i]->set_uniforms(glsl_program_num, effect_id);
	}

	glDisable(GL_BLEND);
	check_error();
	glDisable(GL_DEPTH_TEST);
	check_error();
	glDepthMask(GL_FALSE);
	check_error();

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, 1.0, 0.0, 1.0, 0.0, 1.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glBegin(GL_QUADS);

	glTexCoord2f(0.0f, 1.0f);
	glVertex2f(0.0f, 0.0f);

	glTexCoord2f(1.0f, 1.0f);
	glVertex2f(1.0f, 0.0f);

	glTexCoord2f(1.0f, 0.0f);
	glVertex2f(1.0f, 1.0f);

	glTexCoord2f(0.0f, 0.0f);
	glVertex2f(0.0f, 1.0f);

	glEnd();
	check_error();
}
