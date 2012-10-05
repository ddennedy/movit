#define GL_GLEXT_PROTOTYPES 1

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <GL/gl.h>
#include <GL/glext.h>

#include <algorithm>
#include <set>

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
#include "diffusion_effect.h"

EffectChain::EffectChain(unsigned width, unsigned height)
	: width(width),
	  height(height),
	  use_srgb_texture_format(false),
	  finalized(false) {}

void EffectChain::add_input(const ImageFormat &format)
{
	input_format = format;
	output_color_space.insert(std::make_pair(static_cast<Effect *>(NULL), format.color_space));
	output_gamma_curve.insert(std::make_pair(static_cast<Effect *>(NULL), format.gamma_curve));
	effect_ids.insert(std::make_pair(static_cast<Effect *>(NULL), "src_image"));
}

void EffectChain::add_output(const ImageFormat &format)
{
	output_format = format;
}

void EffectChain::add_effect_raw(Effect *effect, const std::vector<Effect *> &inputs)
{
	char effect_id[256];
	sprintf(effect_id, "eff%u", (unsigned)effects.size());

	effects.push_back(effect);
	effect_ids.insert(std::make_pair(effect, effect_id));
	assert(inputs.size() == effect->num_inputs());
	for (unsigned i = 0; i < inputs.size(); ++i) {
		if (inputs[i] != NULL) {
			assert(std::find(effects.begin(), effects.end(), inputs[i]) != effects.end());
		}
		outgoing_links[inputs[i]].push_back(effect);
	}
	incoming_links.insert(std::make_pair(effect, inputs));
	output_gamma_curve[effect] = output_gamma_curve[last_added_effect()];
	output_color_space[effect] = output_color_space[last_added_effect()];
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
	case EFFECT_DIFFUSION:
		return new DiffusionEffect();
	}
	assert(false);
}

Effect *EffectChain::normalize_to_linear_gamma(Effect *input)
{
	assert(output_gamma_curve.count(input) != 0);
	if (output_gamma_curve[input] == GAMMA_sRGB) {
		// TODO: check if the extension exists
		use_srgb_texture_format = true;
		output_gamma_curve[input] = GAMMA_LINEAR;
		return input;
	} else {
		GammaExpansionEffect *gamma_conversion = new GammaExpansionEffect();
		gamma_conversion->set_int("source_curve", output_gamma_curve[input]);
		std::vector<Effect *> inputs;
		inputs.push_back(input);
		gamma_conversion->add_self_to_effect_chain(this, inputs);
		output_gamma_curve[gamma_conversion] = GAMMA_LINEAR;
		return gamma_conversion;
	}
}

Effect *EffectChain::normalize_to_srgb(Effect *input)
{
	assert(output_gamma_curve.count(input) != 0);
	assert(output_color_space.count(input) != 0);
	assert(output_gamma_curve[input] == GAMMA_LINEAR);
	ColorSpaceConversionEffect *colorspace_conversion = new ColorSpaceConversionEffect();
	colorspace_conversion->set_int("source_space", output_color_space[input]);
	colorspace_conversion->set_int("destination_space", COLORSPACE_sRGB);
	std::vector<Effect *> inputs;
	inputs.push_back(input);
	colorspace_conversion->add_self_to_effect_chain(this, inputs);
	output_color_space[colorspace_conversion] = COLORSPACE_sRGB;
	return colorspace_conversion;
}

Effect *EffectChain::add_effect(EffectId effect_id, const std::vector<Effect *> &inputs)
{
	Effect *effect = instantiate_effect(effect_id);

	assert(inputs.size() == effect->num_inputs());

	std::vector<Effect *> normalized_inputs = inputs;
	for (unsigned i = 0; i < normalized_inputs.size(); ++i) {
		assert(output_gamma_curve.count(normalized_inputs[i]) != 0);
		if (effect->needs_linear_light() && output_gamma_curve[normalized_inputs[i]] != GAMMA_LINEAR) {
			normalized_inputs[i] = normalize_to_linear_gamma(normalized_inputs[i]);
		}
		assert(output_color_space.count(normalized_inputs[i]) != 0);
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

EffectChain::Phase EffectChain::compile_glsl_program(const std::vector<Effect *> &inputs, const std::vector<Effect *> &effects)
{
	assert(!inputs.empty());
	assert(!effects.empty());

	// Figure out the true set of inputs to this phase. These are the ones
	// that we need somehow but don't calculate ourselves.
	std::set<Effect *> effect_set(effects.begin(), effects.end());
	std::set<Effect *> input_set(inputs.begin(), inputs.end());
	std::vector<Effect *> true_inputs;
	std::set_difference(input_set.begin(), input_set.end(),
		effect_set.begin(), effect_set.end(),
		std::back_inserter(true_inputs));

	bool input_needs_mipmaps = false;
	std::string frag_shader = read_file("header.frag");

	// Create functions for all the texture inputs that we need.
	for (unsigned i = 0; i < true_inputs.size(); ++i) {
		Effect *effect = true_inputs[i];
		assert(effect_ids.count(effect) != 0);
		std::string effect_id = effect_ids[effect];
	
		frag_shader += std::string("uniform sampler2D tex_") + effect_id + ";\n";	
		frag_shader += std::string("vec4 ") + effect_id + "(vec2 tc) {\n";
		if (effect == NULL) {
			// OpenGL's origin is bottom-left, but most graphics software assumes
			// a top-left origin. Thus, for inputs that come from the user,
			// we flip the y coordinate. However, for FBOs, the origin
			// is all correct, so don't do anything.
			frag_shader += "\ttc.y = 1.0f - tc.y;\n";
		}
		frag_shader += "\treturn texture2D(tex_" + effect_id + ", tc);\n";
		frag_shader += "}\n";
		frag_shader += "\n";
	}

	std::string last_effect_id;
	for (unsigned i = 0; i < effects.size(); ++i) {
		Effect *effect = effects[i];
		assert(effect != NULL);
		assert(effect_ids.count(effect) != 0);
		std::string effect_id = effect_ids[effect];
		last_effect_id = effect_id;

		if (incoming_links[effect].size() == 1) {
			frag_shader += std::string("#define INPUT ") + effect_ids[incoming_links[effect][0]] + "\n";
		} else {
			for (unsigned j = 0; j < incoming_links[effect].size(); ++j) {
				char buf[256];
				sprintf(buf, "#define INPUT%d %s\n", j + 1, effect_ids[incoming_links[effect][j]].c_str());
				frag_shader += buf;
			}
		}
	
		frag_shader += "\n";
		frag_shader += std::string("#define FUNCNAME ") + effect_id + "\n";
		frag_shader += replace_prefix(effect->output_convenience_uniforms(), effect_id);
		frag_shader += replace_prefix(effect->output_fragment_shader(), effect_id);
		frag_shader += "#undef PREFIX\n";
		frag_shader += "#undef FUNCNAME\n";
		if (incoming_links[effect].size() == 1) {
			frag_shader += "#undef INPUT\n";
		} else {
			for (unsigned j = 0; j < incoming_links[effect].size(); ++j) {
				char buf[256];
				sprintf(buf, "#undef INPUT%d\n", j + 1);
				frag_shader += buf;
			}
		}
		frag_shader += "\n";

		input_needs_mipmaps |= effect->needs_mipmaps();
	}
	assert(!last_effect_id.empty());
	frag_shader += std::string("#define INPUT ") + last_effect_id + "\n";
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
	phase.inputs = true_inputs;
	phase.effects = effects;

	return phase;
}

// Construct GLSL programs, starting at the given effect and following
// the chain from there. We end a program every time we come to an effect
// marked as "needs texture bounce", one that is used by multiple other
// effects, and of course at the end.
void EffectChain::construct_glsl_programs(Effect *start, std::set<Effect *> *completed_effects)
{
	if (completed_effects->count(start) != 0) {
		// This has already been done for us.
		return;
	}

	std::vector<Effect *> this_phase_inputs;  // Also includes all intermediates; these will be filtered away later.
	std::vector<Effect *> this_phase_effects;
	Effect *node = start;
	for ( ;; ) {  // Termination condition within loop.
		if (node == NULL) {
			this_phase_inputs.push_back(node);
		} else {
			// Check that we have all the inputs we need for this effect.
			// If not, we end the phase here right away; the other side
			// of the input chain will eventually come and pick the effect up.
			assert(incoming_links.count(node) != 0);
			std::vector<Effect *> deps = incoming_links[node];
			assert(!deps.empty());
			bool have_all_deps = true;
			for (unsigned i = 0; i < deps.size(); ++i) {
				if (completed_effects->count(deps[i]) == 0) {
					have_all_deps = false;
					break;
				}
			}
		
			if (!have_all_deps) {
				if (!this_phase_effects.empty()) {
					phases.push_back(compile_glsl_program(this_phase_inputs, this_phase_effects));
				}
				return;
			}
			this_phase_inputs.insert(this_phase_inputs.end(), deps.begin(), deps.end());	
			this_phase_effects.push_back(node);
		}
		completed_effects->insert(node);	

		// Find all the effects that use this one as a direct input.
		if (outgoing_links.count(node) == 0) {
			// End of the line; output.
			phases.push_back(compile_glsl_program(this_phase_inputs, this_phase_effects));
			return;
		}

		std::vector<Effect *> next = outgoing_links[node];
		assert(!next.empty());
		if (next.size() > 1) {
			// More than one effect uses this as the input.
			// The easiest thing to do (and probably also the safest
			// performance-wise in most cases) is to bounce it to a texture
			// and then let the next passes read from that.
			if (node != NULL) {
				phases.push_back(compile_glsl_program(this_phase_inputs, this_phase_effects));
			}

			// Start phases for all the effects that need us (in arbitrary order).
			for (unsigned i = 0; i < next.size(); ++i) {
				construct_glsl_programs(next[i], completed_effects);
			}
			return;
		}
	
		// OK, only one effect uses this as the input. Keep iterating,
		// but first see if it requires a texture bounce; if so, give it
		// one by starting a new phase.
		node = next[0];
		if (node->needs_texture_bounce()) {
			phases.push_back(compile_glsl_program(this_phase_inputs, this_phase_effects));
			this_phase_inputs.clear();
			this_phase_effects.clear();
		}
	}
}

void EffectChain::finalize()
{
	// Add normalizers to get the output format right.
	assert(output_gamma_curve.count(last_added_effect()) != 0);
	assert(output_color_space.count(last_added_effect()) != 0);
	ColorSpace current_color_space = output_color_space[last_added_effect()];  // FIXME
	if (current_color_space != output_format.color_space) {
		ColorSpaceConversionEffect *colorspace_conversion = new ColorSpaceConversionEffect();
		colorspace_conversion->set_int("source_space", current_color_space);
		colorspace_conversion->set_int("destination_space", output_format.color_space);
		std::vector<Effect *> inputs;
		inputs.push_back(last_added_effect());
		colorspace_conversion->add_self_to_effect_chain(this, inputs);
		output_color_space[colorspace_conversion] = output_format.color_space;
	}
	GammaCurve current_gamma_curve = output_gamma_curve[last_added_effect()];  // FIXME
	if (current_gamma_curve != output_format.gamma_curve) {
		if (current_gamma_curve != GAMMA_LINEAR) {
			normalize_to_linear_gamma(last_added_effect());  // FIXME
		}
		assert(current_gamma_curve == GAMMA_LINEAR);
		GammaCompressionEffect *gamma_conversion = new GammaCompressionEffect();
		gamma_conversion->set_int("destination_curve", output_format.gamma_curve);
		std::vector<Effect *> inputs;
		inputs.push_back(last_added_effect());
		gamma_conversion->add_self_to_effect_chain(this, inputs);
		output_gamma_curve[gamma_conversion] = output_format.gamma_curve;
	}

	// Construct all needed GLSL programs, starting at the input.
	std::set<Effect *> completed_effects;
	construct_glsl_programs(NULL, &completed_effects);

	// If we have more than one phase, we need intermediate render-to-texture.
	// Construct an FBO, and then as many textures as we need.
	// We choose the simplest option of having one texture per output,
	// since otherwise this turns into an (albeit simple)
	// register allocation problem.
	if (phases.size() > 1) {
		glGenFramebuffers(1, &fbo);

		for (unsigned i = 0; i < phases.size() - 1; ++i) {
			Effect *output_effect = phases[i].effects.back();
			GLuint temp_texture;
			glGenTextures(1, &temp_texture);
			check_error();
			glBindTexture(GL_TEXTURE_2D, temp_texture);
			check_error();
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			check_error();
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			check_error();
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			check_error();
			effect_output_textures.insert(std::make_pair(output_effect, temp_texture));
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

	std::set<Effect *> generated_mipmaps;
	generated_mipmaps.insert(NULL);  // Already done further up.

	for (unsigned phase = 0; phase < phases.size(); ++phase) {
		glUseProgram(phases[phase].glsl_program_num);
		check_error();

		// Set up inputs for this phase.
		assert(!phases[phase].inputs.empty());
		for (unsigned sampler = 0; sampler < phases[phase].inputs.size(); ++sampler) {
			glActiveTexture(GL_TEXTURE0 + sampler);
			Effect *input = phases[phase].inputs[sampler];
			if (input == NULL) {
				glBindTexture(GL_TEXTURE_2D, source_image_num);
				check_error();
			} else {
				assert(effect_output_textures.count(input) != 0);
				glBindTexture(GL_TEXTURE_2D, effect_output_textures[input]);
				check_error();
			}
			if (phases[phase].input_needs_mipmaps) {
				if (generated_mipmaps.count(input) == 0) {
					glGenerateMipmap(GL_TEXTURE_2D);
					check_error();
					generated_mipmaps.insert(input);
				}
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
				check_error();
			} else {
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				check_error();
			}

			assert(effect_ids.count(input));
			std::string texture_name = std::string("tex_") + effect_ids[input];
			glUniform1i(glGetUniformLocation(phases[phase].glsl_program_num, texture_name.c_str()), sampler);
			check_error();
		}

		// And now the output.
		if (phase == phases.size() - 1) {
			// Last phase goes directly to the screen.
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			check_error();
		} else {
			Effect *last_effect = phases[phase].effects.back();
			assert(effect_output_textures.count(last_effect) != 0);
			glFramebufferTexture2D(
				GL_FRAMEBUFFER,
			        GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_2D,
				effect_output_textures[last_effect],
				0);
			check_error();
		}

		// Give the required parameters to all the effects.
		unsigned sampler_num = phases[phase].inputs.size();
		for (unsigned i = 0; i < phases[phase].effects.size(); ++i) {
			Effect *effect = phases[phase].effects[i];
			effect->set_uniforms(phases[phase].glsl_program_num, effect_ids[effect], &sampler_num);
		}

		// Now draw!
		glBegin(GL_QUADS);

		glTexCoord2f(0.0f, 0.0f);
		glVertex2f(0.0f, 0.0f);

		glTexCoord2f(1.0f, 0.0f);
		glVertex2f(1.0f, 0.0f);

		glTexCoord2f(1.0f, 1.0f);
		glVertex2f(1.0f, 1.0f);

		glTexCoord2f(0.0f, 1.0f);
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
