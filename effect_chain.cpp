#define GL_GLEXT_PROTOTYPES 1

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <algorithm>
#include <set>
#include <stack>
#include <vector>

#include "util.h"
#include "effect_chain.h"
#include "gamma_expansion_effect.h"
#include "gamma_compression_effect.h"
#include "colorspace_conversion_effect.h"
#include "input.h"
#include "opengl.h"

EffectChain::EffectChain(unsigned width, unsigned height)
	: width(width),
	  height(height),
	  finalized(false) {}

Input *EffectChain::add_input(Input *input)
{
	char eff_id[256];
	sprintf(eff_id, "src_image%u", (unsigned)inputs.size());

	effects.push_back(input);
	inputs.push_back(input);
	output_color_space.insert(std::make_pair(input, input->get_color_space()));
	output_gamma_curve.insert(std::make_pair(input, input->get_gamma_curve()));
	effect_ids.insert(std::make_pair(input, eff_id));
	incoming_links.insert(std::make_pair(input, std::vector<Effect *>()));
	return input;
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
		assert(std::find(effects.begin(), effects.end(), inputs[i]) != effects.end());
		outgoing_links[inputs[i]].push_back(effect);
	}
	incoming_links.insert(std::make_pair(effect, inputs));
	output_gamma_curve[effect] = output_gamma_curve[last_added_effect()];
	output_color_space[effect] = output_color_space[last_added_effect()];
}

void EffectChain::find_all_nonlinear_inputs(Effect *effect,
                                            std::vector<Input *> *nonlinear_inputs,
                                            std::vector<Effect *> *intermediates)
{
	assert(output_gamma_curve.count(effect) != 0);
	if (output_gamma_curve[effect] == GAMMA_LINEAR) {
		return;
	}
	if (effect->num_inputs() == 0) {
		nonlinear_inputs->push_back(static_cast<Input *>(effect));
	} else {
		intermediates->push_back(effect);

		assert(incoming_links.count(effect) == 1);
		std::vector<Effect *> deps = incoming_links[effect];
		assert(effect->num_inputs() == deps.size());
		for (unsigned i = 0; i < deps.size(); ++i) {
			find_all_nonlinear_inputs(deps[i], nonlinear_inputs, intermediates);
		}
	}
}

Effect *EffectChain::normalize_to_linear_gamma(Effect *input)
{
	// Find out if all the inputs can be set to deliver sRGB inputs.
	// If so, we can just ask them to do that instead of inserting a
	// (possibly expensive) conversion operation.
	//
	// NOTE: We assume that effects generally don't mess with the gamma
	// curve (except GammaCompressionEffect, which should never be
	// inserted into a chain when this is called), so that we can just
	// update the output gamma as we go.
	//
	// TODO: Setting this flag for one source might confuse a different
	// part of the pipeline using the same source.
	std::vector<Input *> nonlinear_inputs;
	std::vector<Effect *> intermediates;
	find_all_nonlinear_inputs(input, &nonlinear_inputs, &intermediates);

	bool all_ok = true;
	for (unsigned i = 0; i < nonlinear_inputs.size(); ++i) {
		all_ok &= nonlinear_inputs[i]->can_output_linear_gamma();
	}

	if (all_ok) {
		for (unsigned i = 0; i < nonlinear_inputs.size(); ++i) {
			bool ok = nonlinear_inputs[i]->set_int("output_linear_gamma", 1);
			assert(ok);
			output_gamma_curve[nonlinear_inputs[i]] = GAMMA_LINEAR;
		}
		for (unsigned i = 0; i < intermediates.size(); ++i) {
			output_gamma_curve[intermediates[i]] = GAMMA_LINEAR;
		}
		return input;
	}

	// OK, that didn't work. Insert a conversion effect.
	GammaExpansionEffect *gamma_conversion = new GammaExpansionEffect();
	gamma_conversion->set_int("source_curve", output_gamma_curve[input]);
	std::vector<Effect *> inputs;
	inputs.push_back(input);
	gamma_conversion->add_self_to_effect_chain(this, inputs);
	output_gamma_curve[gamma_conversion] = GAMMA_LINEAR;
	return gamma_conversion;
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

Effect *EffectChain::add_effect(Effect *effect, const std::vector<Effect *> &inputs)
{
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

EffectChain::Phase *EffectChain::compile_glsl_program(const std::vector<Effect *> &inputs, const std::vector<Effect *> &effects)
{
	assert(!effects.empty());

	// Deduplicate the inputs.
	std::vector<Effect *> true_inputs = inputs;
	std::sort(true_inputs.begin(), true_inputs.end());
	true_inputs.erase(std::unique(true_inputs.begin(), true_inputs.end()), true_inputs.end());

	bool input_needs_mipmaps = false;
	std::string frag_shader = read_file("header.frag");

	// Create functions for all the texture inputs that we need.
	for (unsigned i = 0; i < true_inputs.size(); ++i) {
		Effect *effect = true_inputs[i];
		assert(effect_ids.count(effect) != 0);
		std::string effect_id = effect_ids[effect];
	
		frag_shader += std::string("uniform sampler2D tex_") + effect_id + ";\n";	
		frag_shader += std::string("vec4 ") + effect_id + "(vec2 tc) {\n";
		if (effect->num_inputs() == 0) {
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
	for (unsigned i = 0; i < effects.size(); ++i) {
		Effect *effect = effects[i];
		if (effect->num_inputs() == 0) {
			effect->set_int("needs_mipmaps", input_needs_mipmaps);
		}
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

	Phase *phase = new Phase;
	phase->glsl_program_num = glsl_program_num;
	phase->input_needs_mipmaps = input_needs_mipmaps;
	phase->inputs = true_inputs;
	phase->effects = effects;

	return phase;
}

// Construct GLSL programs, starting at the given effect and following
// the chain from there. We end a program every time we come to an effect
// marked as "needs texture bounce", one that is used by multiple other
// effects, every time an effect wants to change the output size,
// and of course at the end.
//
// We follow a quite simple depth-first search from the output, although
// without any explicit recursion.
void EffectChain::construct_glsl_programs(Effect *output)
{
	// Which effects have already been completed in this phase?
	// We need to keep track of it, as an effect with multiple outputs
	// could otherwise be calculate multiple times.
	std::set<Effect *> completed_effects;

	// Effects in the current phase, as well as inputs (outputs from other phases
	// that we depend on). Note that since we start iterating from the end,
	// the effect list will be in the reverse order.
	std::vector<Effect *> this_phase_inputs;
	std::vector<Effect *> this_phase_effects;

	// Effects that we have yet to calculate, but that we know should
	// be in the current phase.
	std::stack<Effect *> effects_todo_this_phase;

	// Effects that we have yet to calculate, but that come from other phases.
	// We delay these until we have this phase done in its entirety,
	// at which point we pick any of them and start a new phase from that.
	std::stack<Effect *> effects_todo_other_phases;

	effects_todo_this_phase.push(output);

	for ( ;; ) {  // Termination condition within loop.
		if (!effects_todo_this_phase.empty()) {
			// OK, we have more to do this phase.
			Effect *effect = effects_todo_this_phase.top();
			effects_todo_this_phase.pop();

			// This should currently only happen for effects that are phase outputs,
			// and we throw those out separately below.
			assert(completed_effects.count(effect) == 0);

			this_phase_effects.push_back(effect);
			completed_effects.insert(effect);

			// Find all the dependencies of this effect, and add them to the stack.
			assert(incoming_links.count(effect) == 1);
			std::vector<Effect *> deps = incoming_links[effect];
			assert(effect->num_inputs() == deps.size());
			for (unsigned i = 0; i < deps.size(); ++i) {
				bool start_new_phase = false;

				// FIXME: If we sample directly from a texture, we won't need this.
				if (effect->needs_texture_bounce()) {
					start_new_phase = true;
				}

				assert(outgoing_links.count(deps[i]) == 1);
				if (outgoing_links[deps[i]].size() > 1 && deps[i]->num_inputs() > 0) {
					// More than one effect uses this as the input,
					// and it is not a texture itself.
					// The easiest thing to do (and probably also the safest
					// performance-wise in most cases) is to bounce it to a texture
					// and then let the next passes read from that.
					start_new_phase = true;
				}

				if (deps[i]->changes_output_size()) {
					start_new_phase = true;
				}

				if (start_new_phase) {
					effects_todo_other_phases.push(deps[i]);
					this_phase_inputs.push_back(deps[i]);
				} else {
					effects_todo_this_phase.push(deps[i]);
				}
			}
			continue;
		}

		// No more effects to do this phase. Take all the ones we have,
		// and create a GLSL program for it.
		if (!this_phase_effects.empty()) {
			reverse(this_phase_effects.begin(), this_phase_effects.end());
			phases.push_back(compile_glsl_program(this_phase_inputs, this_phase_effects));
			output_effects_to_phase.insert(std::make_pair(this_phase_effects.back(), phases.back()));
			this_phase_inputs.clear();
			this_phase_effects.clear();
		}
		assert(this_phase_inputs.empty());
		assert(this_phase_effects.empty());

		// If we have no effects left, exit.
		if (effects_todo_other_phases.empty()) {
			break;
		}

		Effect *effect = effects_todo_other_phases.top();
		effects_todo_other_phases.pop();

		if (completed_effects.count(effect) == 0) {
			// Start a new phase, calculating from this effect.
			effects_todo_this_phase.push(effect);
		}
	}

	// Finally, since the phases are found from the output but must be executed
	// from the input(s), reverse them, too.
	std::reverse(phases.begin(), phases.end());
}

void EffectChain::find_output_size(EffectChain::Phase *phase)
{
	Effect *output_effect = phase->effects.back();

	// If the last effect explicitly sets an output size,
	// use that.
	if (output_effect->changes_output_size()) {
		output_effect->get_output_size(&phase->output_width, &phase->output_height);
		return;
	}

	// If not, look at the input phases, if any. We select the largest one
	// (really assuming they all have the same aspect currently), by pixel count.
	if (!phase->inputs.empty()) {
		unsigned best_width = 0, best_height = 0;
		for (unsigned i = 0; i < phase->inputs.size(); ++i) {
			Effect *input = phase->inputs[i];
			assert(output_effects_to_phase.count(input) != 0);
			const Phase *input_phase = output_effects_to_phase[input];
			assert(input_phase->output_width != 0);
			assert(input_phase->output_height != 0);
			if (input_phase->output_width * input_phase->output_height > best_width * best_height) {
				best_width = input_phase->output_width;
				best_height = input_phase->output_height;
			}
		}
		assert(best_width != 0);
		assert(best_height != 0);
		phase->output_width = best_width;
		phase->output_height = best_height;
		return;
	}

	// OK, no inputs. Just use the global width/height.
	// TODO: We probably want to use the texture's size eventually.
	phase->output_width = width;
	phase->output_height = height;
}

void EffectChain::finalize()
{
	// Find the output effect. This is, simply, one that has no outgoing links.
	// If there are multiple ones, the graph is malformed (we do not support
	// multiple outputs right now).
	std::vector<Effect *> output_effects;
	for (unsigned i = 0; i < effects.size(); ++i) {
		Effect *effect = effects[i];
		if (outgoing_links.count(effect) == 0 || outgoing_links[effect].size() == 0) {
			output_effects.push_back(effect);
		}
	}
	assert(output_effects.size() == 1);
	Effect *output_effect = output_effects[0];

	// Add normalizers to get the output format right.
	assert(output_gamma_curve.count(output_effect) != 0);
	assert(output_color_space.count(output_effect) != 0);
	ColorSpace current_color_space = output_color_space[output_effect];
	if (current_color_space != output_format.color_space) {
		ColorSpaceConversionEffect *colorspace_conversion = new ColorSpaceConversionEffect();
		colorspace_conversion->set_int("source_space", current_color_space);
		colorspace_conversion->set_int("destination_space", output_format.color_space);
		std::vector<Effect *> inputs;
		inputs.push_back(output_effect);
		colorspace_conversion->add_self_to_effect_chain(this, inputs);
		output_color_space[colorspace_conversion] = output_format.color_space;
		output_effect = colorspace_conversion;
	}
	GammaCurve current_gamma_curve = output_gamma_curve[output_effect];
	if (current_gamma_curve != output_format.gamma_curve) {
		if (current_gamma_curve != GAMMA_LINEAR) {
			output_effect = normalize_to_linear_gamma(output_effect);
			current_gamma_curve = GAMMA_LINEAR;
		}
		GammaCompressionEffect *gamma_conversion = new GammaCompressionEffect();
		gamma_conversion->set_int("destination_curve", output_format.gamma_curve);
		std::vector<Effect *> inputs;
		inputs.push_back(output_effect);
		gamma_conversion->add_self_to_effect_chain(this, inputs);
		output_gamma_curve[gamma_conversion] = output_format.gamma_curve;
		output_effect = gamma_conversion;
	}

	// Construct all needed GLSL programs, starting at the output.
	construct_glsl_programs(output_effect);

	// If we have more than one phase, we need intermediate render-to-texture.
	// Construct an FBO, and then as many textures as we need.
	// We choose the simplest option of having one texture per output,
	// since otherwise this turns into an (albeit simple)
	// register allocation problem.
	if (phases.size() > 1) {
		glGenFramebuffers(1, &fbo);

		for (unsigned i = 0; i < phases.size() - 1; ++i) {
			find_output_size(phases[i]);

			Effect *output_effect = phases[i]->effects.back();
			GLuint temp_texture;
			glGenTextures(1, &temp_texture);
			check_error();
			glBindTexture(GL_TEXTURE_2D, temp_texture);
			check_error();
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			check_error();
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			check_error();
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, phases[i]->output_width, phases[i]->output_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			check_error();
			effect_output_textures.insert(std::make_pair(output_effect, temp_texture));
			effect_output_texture_sizes.insert(std::make_pair(output_effect, std::make_pair(phases[i]->output_width, phases[i]->output_height)));
		}
	}
		
	for (unsigned i = 0; i < inputs.size(); ++i) {
		inputs[i]->finalize();
	}

	assert(phases[0]->inputs.empty());
	
	finalized = true;
}

void EffectChain::render_to_screen()
{
	assert(finalized);

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

	for (unsigned phase = 0; phase < phases.size(); ++phase) {
		// See if the requested output size has changed. If so, we need to recreate
		// the texture (and before we start setting up inputs).
		if (phase != phases.size() - 1) {
			find_output_size(phases[phase]);

			Effect *output_effect = phases[phase]->effects.back();
			assert(effect_output_texture_sizes.count(output_effect) != 0);
			std::pair<GLuint, GLuint> old_size = effect_output_texture_sizes[output_effect];

			if (old_size.first != phases[phase]->output_width ||
			    old_size.second != phases[phase]->output_height) {
				glActiveTexture(GL_TEXTURE0);
				check_error();
				assert(effect_output_textures.count(output_effect) != 0);
				glBindTexture(GL_TEXTURE_2D, effect_output_textures[output_effect]);
				check_error();
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, phases[phase]->output_width, phases[phase]->output_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
				check_error();
				effect_output_texture_sizes[output_effect] = std::make_pair(phases[phase]->output_width, phases[phase]->output_height);
				glBindTexture(GL_TEXTURE_2D, 0);
				check_error();
			}
		}

		glUseProgram(phases[phase]->glsl_program_num);
		check_error();

		// Set up RTT inputs for this phase.
		for (unsigned sampler = 0; sampler < phases[phase]->inputs.size(); ++sampler) {
			glActiveTexture(GL_TEXTURE0 + sampler);
			Effect *input = phases[phase]->inputs[sampler];
			assert(effect_output_textures.count(input) != 0);
			glBindTexture(GL_TEXTURE_2D, effect_output_textures[input]);
			check_error();
			if (phases[phase]->input_needs_mipmaps) {
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
			glUniform1i(glGetUniformLocation(phases[phase]->glsl_program_num, texture_name.c_str()), sampler);
			check_error();
		}

		// And now the output.
		if (phase == phases.size() - 1) {
			// Last phase goes directly to the screen.
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			check_error();
			glViewport(0, 0, width, height);
		} else {
			Effect *output_effect = phases[phase]->effects.back();
			assert(effect_output_textures.count(output_effect) != 0);
			glFramebufferTexture2D(
				GL_FRAMEBUFFER,
			        GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_2D,
				effect_output_textures[output_effect],
				0);
			check_error();
			glViewport(0, 0, phases[phase]->output_width, phases[phase]->output_height);
		}

		// Give the required parameters to all the effects.
		unsigned sampler_num = phases[phase]->inputs.size();
		for (unsigned i = 0; i < phases[phase]->effects.size(); ++i) {
			Effect *effect = phases[phase]->effects[i];
			effect->set_gl_state(phases[phase]->glsl_program_num, effect_ids[effect], &sampler_num);
			check_error();
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

		for (unsigned i = 0; i < phases[phase]->effects.size(); ++i) {
			Effect *effect = phases[phase]->effects[i];
			effect->clear_gl_state();
		}
	}
}
