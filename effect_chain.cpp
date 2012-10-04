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
#include "sandbox_effect.h"
#include "saturation_effect.h"
#include "mirror_effect.h"
#include "vignette_effect.h"
#include "blur_effect.h"

EffectChain::EffectChain(unsigned width, unsigned height)
	: width(width),
	  height(height),
	  last_added_effect(NULL),
	  use_srgb_texture_format(false),
	  finalized(false) {}

void EffectChain::add_input(const ImageFormat &format)
{
	input_format = format;
	output_color_space.insert(std::make_pair(static_cast<Effect *>(NULL), format.color_space));
	output_gamma_curve.insert(std::make_pair(static_cast<Effect *>(NULL), format.gamma_curve));
}

void EffectChain::add_output(const ImageFormat &format)
{
	output_format = format;
}

void EffectChain::add_effect_raw(Effect *effect, const std::vector<Effect *> &inputs)
{
	effects.push_back(effect);
	assert(inputs.size() == effect->num_inputs());
	for (unsigned i = 0; i < inputs.size(); ++i) {
		outgoing_links.insert(std::make_pair(inputs[i], effect));
		incoming_links.insert(std::make_pair(effect, inputs[i]));
	}
	last_added_effect = effect;
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
	case EFFECT_SANDBOX:
		return new SandboxEffect();
	case EFFECT_LIFT_GAMMA_GAIN:
		return new LiftGammaGainEffect();
	case EFFECT_SATURATION:
		return new SaturationEffect();
	case EFFECT_MIRROR:
		return new MirrorEffect();
	case EFFECT_VIGNETTE:
		return new VignetteEffect();
	case EFFECT_BLUR:
		return new BlurEffect();
	}
	assert(false);
}

Effect *EffectChain::normalize_to_linear_gamma(Effect *input)
{
	GammaCurve current_gamma_curve = output_gamma_curve[input];
	if (current_gamma_curve == GAMMA_sRGB) {
		// TODO: check if the extension exists
		use_srgb_texture_format = true;
		current_gamma_curve = GAMMA_LINEAR;
		return input;
	} else {
		GammaExpansionEffect *gamma_conversion = new GammaExpansionEffect();
		gamma_conversion->set_int("source_curve", current_gamma_curve);
		std::vector<Effect *> inputs;
		inputs.push_back(input);
		gamma_conversion->add_self_to_effect_chain(this, inputs);
		current_gamma_curve = GAMMA_LINEAR;
		return gamma_conversion;
	}
}

Effect *EffectChain::normalize_to_srgb(Effect *input)
{
	GammaCurve current_gamma_curve = output_gamma_curve[input];
	ColorSpace current_color_space = output_color_space[input];
	assert(current_gamma_curve == GAMMA_LINEAR);
	ColorSpaceConversionEffect *colorspace_conversion = new ColorSpaceConversionEffect();
	colorspace_conversion->set_int("source_space", current_color_space);
	colorspace_conversion->set_int("destination_space", COLORSPACE_sRGB);
	std::vector<Effect *> inputs;
	inputs.push_back(input);
	colorspace_conversion->add_self_to_effect_chain(this, inputs);
	current_color_space = COLORSPACE_sRGB;
	return colorspace_conversion;
}

Effect *EffectChain::add_effect(EffectId effect_id, const std::vector<Effect *> &inputs)
{
	Effect *effect = instantiate_effect(effect_id);

	assert(inputs.size() == effect->num_inputs());

	std::vector<Effect *> normalized_inputs = inputs;
	for (unsigned i = 0; i < normalized_inputs.size(); ++i) {
		if (effect->needs_linear_light() && output_gamma_curve[normalized_inputs[i]] != GAMMA_LINEAR) {
			normalized_inputs[i] = normalize_to_linear_gamma(normalized_inputs[i]);
		}
		if (effect->needs_srgb_primaries() && output_color_space[normalized_inputs[i]] != COLORSPACE_sRGB) {
			normalized_inputs[i] = normalize_to_srgb(normalized_inputs[i]);
		}
	}

	effect->add_self_to_effect_chain(this, normalized_inputs);
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

EffectChain::Phase EffectChain::compile_glsl_program(unsigned start_index, unsigned end_index)
{
	bool input_needs_mipmaps = false;
	std::string frag_shader = read_file("header.frag");
	for (unsigned i = start_index; i < end_index; ++i) {
		char effect_id[256];
		sprintf(effect_id, "eff%d", i);
	
		frag_shader += "\n";
		frag_shader += std::string("#define FUNCNAME ") + effect_id + "\n";
		frag_shader += replace_prefix(effects[i]->output_convenience_uniforms(), effect_id);
		frag_shader += replace_prefix(effects[i]->output_fragment_shader(), effect_id);
		frag_shader += "#undef PREFIX\n";
		frag_shader += "#undef FUNCNAME\n";
		frag_shader += "#undef INPUT\n";
		frag_shader += std::string("#define INPUT ") + effect_id + "\n";
		frag_shader += "\n";

		input_needs_mipmaps |= effects[i]->needs_mipmaps();
	}
	frag_shader.append(read_file("footer.frag"));
	printf("%s\n", frag_shader.c_str());
	
	GLuint glsl_program_num = glCreateProgram();
	GLuint vs_obj = compile_shader(read_file("vs.vert"), GL_VERTEX_SHADER);
	GLuint fs_obj = compile_shader(frag_shader, GL_FRAGMENT_SHADER);
	glAttachShader(glsl_program_num, vs_obj);
	check_error();
	glAttachShader(glsl_program_num, fs_obj);
	check_error();
	glLinkProgram(glsl_program_num);
	check_error();

	Phase phase;
	phase.glsl_program_num = glsl_program_num;
	phase.input_needs_mipmaps = input_needs_mipmaps;
	phase.start = start_index;
	phase.end = end_index;

	return phase;
}

void EffectChain::finalize()
{
	// Add normalizers to get the output format right.
	GammaCurve current_gamma_curve = output_gamma_curve[last_added_effect];  // FIXME
	ColorSpace current_color_space = output_color_space[last_added_effect];  // FIXME
	if (current_color_space != output_format.color_space) {
		ColorSpaceConversionEffect *colorspace_conversion = new ColorSpaceConversionEffect();
		colorspace_conversion->set_int("source_space", current_color_space);
		colorspace_conversion->set_int("destination_space", output_format.color_space);
		effects.push_back(colorspace_conversion);
		current_color_space = output_format.color_space;
	}
	if (current_gamma_curve != output_format.gamma_curve) {
		if (current_gamma_curve != GAMMA_LINEAR) {
			normalize_to_linear_gamma(last_added_effect);  // FIXME
		}
		assert(current_gamma_curve == GAMMA_LINEAR);
		GammaCompressionEffect *gamma_conversion = new GammaCompressionEffect();
		gamma_conversion->set_int("destination_curve", output_format.gamma_curve);
		effects.push_back(gamma_conversion);
		current_gamma_curve = output_format.gamma_curve;
	}

	// Construct the GLSL programs. We end a program every time we come
	// to an effect marked as "needs many samples" (ie. "please let me
	// sample directly from a texture, with no arithmetic in-between"),
	// and of course at the end.
	unsigned start = 0;
	for (unsigned i = 0; i < effects.size(); ++i) {
		if (effects[i]->needs_texture_bounce() && i != start) {
			phases.push_back(compile_glsl_program(start, i));
			start = i;
		}
	}
	phases.push_back(compile_glsl_program(start, effects.size()));

	// If we have more than one phase, we need intermediate render-to-texture.
	// Construct an FBO, and then as many textures as we need.
	if (phases.size() > 1) {
		glGenFramebuffers(1, &fbo);

		unsigned num_textures = std::max<int>(phases.size() - 1, 2);
		glGenTextures(num_textures, temp_textures);

		for (unsigned i = 0; i < num_textures; ++i) {
			glBindTexture(GL_TEXTURE_2D, temp_textures[i]);
			check_error();
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			check_error();
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			check_error();
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			check_error();
		}
	}
	
	// Translate the input format to OpenGL's enums.
	GLenum internal_format;
	if (use_srgb_texture_format) {
		internal_format = GL_SRGB8;
	} else {
		internal_format = GL_RGBA8;
	}
	if (input_format.pixel_format == FORMAT_RGB) {
		format = GL_RGB;
		bytes_per_pixel = 3;
	} else if (input_format.pixel_format == FORMAT_RGBA) {
		format = GL_RGBA;
		bytes_per_pixel = 4;
	} else if (input_format.pixel_format == FORMAT_BGR) {
		format = GL_BGR;
		bytes_per_pixel = 3;
	} else if (input_format.pixel_format == FORMAT_BGRA) {
		format = GL_BGRA;
		bytes_per_pixel = 4;
	} else {
		assert(false);
	}

	// Create PBO to hold the texture holding the input image, and then the texture itself.
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 2);
	check_error();
	glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, width * height * bytes_per_pixel, NULL, GL_STREAM_DRAW);
	check_error();
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
	check_error();
	
	glGenTextures(1, &source_image_num);
	check_error();
	glBindTexture(GL_TEXTURE_2D, source_image_num);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	check_error();
	// Intel/Mesa seems to have a broken glGenerateMipmap() for non-FBO textures, so do it here.
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, phases[0].input_needs_mipmaps ? GL_TRUE : GL_FALSE);
	check_error();
	glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, GL_UNSIGNED_BYTE, NULL);
	check_error();

	finalized = true;
}

void EffectChain::render_to_screen(unsigned char *src)
{
	assert(finalized);

	// Copy the pixel data into the PBO.
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 2);
	check_error();
	void *mapped_pbo = glMapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY);
	memcpy(mapped_pbo, src, width * height * bytes_per_pixel);
	glUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB);
	check_error();

	// Re-upload the texture from the PBO.
	glActiveTexture(GL_TEXTURE0);
	check_error();
	glBindTexture(GL_TEXTURE_2D, source_image_num);
	check_error();
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, GL_UNSIGNED_BYTE, BUFFER_OFFSET(0));
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	check_error();
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
	check_error();

	// Basic state.
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

	if (phases.size() > 1) {
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		check_error();
	}

	for (unsigned phase = 0; phase < phases.size(); ++phase) {
		// Set up inputs and outputs for this phase.
		glActiveTexture(GL_TEXTURE0);
		if (phase == 0) {
			// First phase reads from the input texture (which is already bound).
		} else {
			glBindTexture(GL_TEXTURE_2D, temp_textures[(phase + 1) % 2]);
			check_error();
		}
		if (phases[phase].input_needs_mipmaps) {
			if (phase != 0) {
				// For phase 0, it's done further up.
				glGenerateMipmap(GL_TEXTURE_2D);
				check_error();
			}
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
			check_error();
		} else {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			check_error();
		}

		if (phase == phases.size() - 1) {
			// Last phase goes directly to the screen.
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			check_error();
		} else {
			glFramebufferTexture2D(
				GL_FRAMEBUFFER,
			        GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_2D,
				temp_textures[phase % 2],
				0);
			check_error();
		}

		// We have baked an upside-down transform into the quad coordinates,
		// since the typical graphics program will have the origin at the upper-left,
		// while OpenGL uses lower-left. In the next ones, however, the origin
		// is all right, and we need to reverse that.
		if (phase == 1) {
			glTranslatef(0.0f, 1.0f, 0.0f);
			glScalef(1.0f, -1.0f, 1.0f);
		}

		// Give the required parameters to all the effects.
		glUseProgram(phases[phase].glsl_program_num);
		check_error();

		glUniform1i(glGetUniformLocation(phases[phase].glsl_program_num, "input_tex"), 0);
		check_error();

		unsigned sampler_num = 1;
		for (unsigned i = phases[phase].start; i < phases[phase].end; ++i) {
			char effect_id[256];
			sprintf(effect_id, "eff%d", i);
			effects[i]->set_uniforms(phases[phase].glsl_program_num, effect_id, &sampler_num);
		}

		// Now draw!
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

		// HACK
		glActiveTexture(GL_TEXTURE0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		check_error();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1000);
		check_error();
	}
}
