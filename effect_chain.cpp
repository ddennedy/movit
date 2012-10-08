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

	inputs.push_back(input);

	Node *node = new Node;
	node->effect = input;
	node->effect_id = eff_id;
	node->output_color_space = input->get_color_space();
	node->output_gamma_curve = input->get_gamma_curve();

	nodes.push_back(node);
	node_map[input] = node;

	return input;
}

void EffectChain::add_output(const ImageFormat &format)
{
	output_format = format;
}

void EffectChain::add_effect_raw(Effect *effect, const std::vector<Effect *> &inputs)
{
	char effect_id[256];
	sprintf(effect_id, "eff%u", (unsigned)nodes.size());

	Node *node = new Node;
	node->effect = effect;
	node->effect_id = effect_id;

	assert(inputs.size() == effect->num_inputs());
	assert(inputs.size() >= 1);
	for (unsigned i = 0; i < inputs.size(); ++i) {
		assert(node_map.count(inputs[i]) != 0);
		node_map[inputs[i]]->outgoing_links.push_back(node);
		node->incoming_links.push_back(node_map[inputs[i]]);
		if (i == 0) {
			node->output_gamma_curve = node_map[inputs[i]]->output_gamma_curve;
			node->output_color_space = node_map[inputs[i]]->output_color_space;
		} else {
			assert(node->output_gamma_curve == node_map[inputs[i]]->output_gamma_curve);
			assert(node->output_color_space == node_map[inputs[i]]->output_color_space);
		}
	}

	nodes.push_back(node);
	node_map[effect] = node;
}

void EffectChain::find_all_nonlinear_inputs(Node *node,
                                            std::vector<Node *> *nonlinear_inputs,
                                            std::vector<Node *> *intermediates)
{
	if (node->output_gamma_curve == GAMMA_LINEAR) {
		return;
	}
	if (node->effect->num_inputs() == 0) {
		nonlinear_inputs->push_back(node);
	} else {
		intermediates->push_back(node);
		assert(node->effect->num_inputs() == node->incoming_links.size());
		for (unsigned i = 0; i < node->incoming_links.size(); ++i) {
			find_all_nonlinear_inputs(node->incoming_links[i], nonlinear_inputs, intermediates);
		}
	}
}

Node *EffectChain::normalize_to_linear_gamma(Node *input)
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
	std::vector<Node *> nonlinear_inputs;
	std::vector<Node *> intermediates;
	find_all_nonlinear_inputs(input, &nonlinear_inputs, &intermediates);

	bool all_ok = true;
	for (unsigned i = 0; i < nonlinear_inputs.size(); ++i) {
		Input *input = static_cast<Input *>(nonlinear_inputs[i]->effect);
		all_ok &= input->can_output_linear_gamma();
	}

	if (all_ok) {
		for (unsigned i = 0; i < nonlinear_inputs.size(); ++i) {
			bool ok = nonlinear_inputs[i]->effect->set_int("output_linear_gamma", 1);
			assert(ok);
			nonlinear_inputs[i]->output_gamma_curve = GAMMA_LINEAR;
		}
		for (unsigned i = 0; i < intermediates.size(); ++i) {
			intermediates[i]->output_gamma_curve = GAMMA_LINEAR;
		}
		return input;
	}

	// OK, that didn't work. Insert a conversion effect.
	GammaExpansionEffect *gamma_conversion = new GammaExpansionEffect();
	gamma_conversion->set_int("source_curve", input->output_gamma_curve);
	std::vector<Effect *> inputs;
	inputs.push_back(input->effect);
	gamma_conversion->add_self_to_effect_chain(this, inputs);

	assert(node_map.count(gamma_conversion) != 0);
	Node *node = node_map[gamma_conversion];
	node->output_gamma_curve = GAMMA_LINEAR;
	return node;
}

Node *EffectChain::normalize_to_srgb(Node *input)
{
	assert(input->output_gamma_curve == GAMMA_LINEAR);
	ColorSpaceConversionEffect *colorspace_conversion = new ColorSpaceConversionEffect();
	colorspace_conversion->set_int("source_space", input->output_color_space);
	colorspace_conversion->set_int("destination_space", COLORSPACE_sRGB);
	std::vector<Effect *> inputs;
	inputs.push_back(input->effect);
	colorspace_conversion->add_self_to_effect_chain(this, inputs);

	assert(node_map.count(colorspace_conversion) != 0);
	Node *node = node_map[colorspace_conversion];
	node->output_color_space = COLORSPACE_sRGB;
	return node;
}

Effect *EffectChain::add_effect(Effect *effect, const std::vector<Effect *> &inputs)
{
	assert(inputs.size() == effect->num_inputs());

	std::vector<Effect *> normalized_inputs = inputs;
	for (unsigned i = 0; i < normalized_inputs.size(); ++i) {
		assert(node_map.count(normalized_inputs[i]) != 0);
		Node *input = node_map[normalized_inputs[i]];
		if (effect->needs_linear_light() && input->output_gamma_curve != GAMMA_LINEAR) {
			input = normalize_to_linear_gamma(input);
		}
		if (effect->needs_srgb_primaries() && input->output_color_space != COLORSPACE_sRGB) {
			input = normalize_to_srgb(input);
		}
		normalized_inputs[i] = input->effect;
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

Phase *EffectChain::compile_glsl_program(
	const std::vector<Node *> &inputs,
	const std::vector<Node *> &effects)
{
	assert(!effects.empty());

	// Deduplicate the inputs.
	std::vector<Node *> true_inputs = inputs;
	std::sort(true_inputs.begin(), true_inputs.end());
	true_inputs.erase(std::unique(true_inputs.begin(), true_inputs.end()), true_inputs.end());

	bool input_needs_mipmaps = false;
	std::string frag_shader = read_file("header.frag");

	// Create functions for all the texture inputs that we need.
	for (unsigned i = 0; i < true_inputs.size(); ++i) {
		Node *input = true_inputs[i];
	
		frag_shader += std::string("uniform sampler2D tex_") + input->effect_id + ";\n";
		frag_shader += std::string("vec4 ") + input->effect_id + "(vec2 tc) {\n";
		frag_shader += "\treturn texture2D(tex_" + input->effect_id + ", tc);\n";
		frag_shader += "}\n";
		frag_shader += "\n";
	}

	for (unsigned i = 0; i < effects.size(); ++i) {
		Node *node = effects[i];

		if (node->incoming_links.size() == 1) {
			frag_shader += std::string("#define INPUT ") + node->incoming_links[0]->effect_id + "\n";
		} else {
			for (unsigned j = 0; j < node->incoming_links.size(); ++j) {
				char buf[256];
				sprintf(buf, "#define INPUT%d %s\n", j + 1, node->incoming_links[j]->effect_id.c_str());
				frag_shader += buf;
			}
		}
	
		frag_shader += "\n";
		frag_shader += std::string("#define FUNCNAME ") + node->effect_id + "\n";
		frag_shader += replace_prefix(node->effect->output_convenience_uniforms(), node->effect_id);
		frag_shader += replace_prefix(node->effect->output_fragment_shader(), node->effect_id);
		frag_shader += "#undef PREFIX\n";
		frag_shader += "#undef FUNCNAME\n";
		if (node->incoming_links.size() == 1) {
			frag_shader += "#undef INPUT\n";
		} else {
			for (unsigned j = 0; j < node->incoming_links.size(); ++j) {
				char buf[256];
				sprintf(buf, "#undef INPUT%d\n", j + 1);
				frag_shader += buf;
			}
		}
		frag_shader += "\n";

		input_needs_mipmaps |= node->effect->needs_mipmaps();
	}
	for (unsigned i = 0; i < effects.size(); ++i) {
		Node *node = effects[i];
		if (node->effect->num_inputs() == 0) {
			node->effect->set_int("needs_mipmaps", input_needs_mipmaps);
		}
	}
	frag_shader += std::string("#define INPUT ") + effects.back()->effect_id + "\n";
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
void EffectChain::construct_glsl_programs(Node *output)
{
	// Which effects have already been completed in this phase?
	// We need to keep track of it, as an effect with multiple outputs
	// could otherwise be calculate multiple times.
	std::set<Node *> completed_effects;

	// Effects in the current phase, as well as inputs (outputs from other phases
	// that we depend on). Note that since we start iterating from the end,
	// the effect list will be in the reverse order.
	std::vector<Node *> this_phase_inputs;
	std::vector<Node *> this_phase_effects;

	// Effects that we have yet to calculate, but that we know should
	// be in the current phase.
	std::stack<Node *> effects_todo_this_phase;

	// Effects that we have yet to calculate, but that come from other phases.
	// We delay these until we have this phase done in its entirety,
	// at which point we pick any of them and start a new phase from that.
	std::stack<Node *> effects_todo_other_phases;

	effects_todo_this_phase.push(output);

	for ( ;; ) {  // Termination condition within loop.
		if (!effects_todo_this_phase.empty()) {
			// OK, we have more to do this phase.
			Node *node = effects_todo_this_phase.top();
			effects_todo_this_phase.pop();

			// This should currently only happen for effects that are phase outputs,
			// and we throw those out separately below.
			assert(completed_effects.count(node) == 0);

			this_phase_effects.push_back(node);
			completed_effects.insert(node);

			// Find all the dependencies of this effect, and add them to the stack.
			std::vector<Node *> deps = node->incoming_links;
			assert(node->effect->num_inputs() == deps.size());
			for (unsigned i = 0; i < deps.size(); ++i) {
				bool start_new_phase = false;

				// FIXME: If we sample directly from a texture, we won't need this.
				if (node->effect->needs_texture_bounce()) {
					start_new_phase = true;
				}

				if (deps[i]->outgoing_links.size() > 1 && deps[i]->effect->num_inputs() > 0) {
					// More than one effect uses this as the input,
					// and it is not a texture itself.
					// The easiest thing to do (and probably also the safest
					// performance-wise in most cases) is to bounce it to a texture
					// and then let the next passes read from that.
					start_new_phase = true;
				}

				if (deps[i]->effect->changes_output_size()) {
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
			this_phase_effects.back()->phase = phases.back();
			this_phase_inputs.clear();
			this_phase_effects.clear();
		}
		assert(this_phase_inputs.empty());
		assert(this_phase_effects.empty());

		// If we have no effects left, exit.
		if (effects_todo_other_phases.empty()) {
			break;
		}

		Node *node = effects_todo_other_phases.top();
		effects_todo_other_phases.pop();

		if (completed_effects.count(node) == 0) {
			// Start a new phase, calculating from this effect.
			effects_todo_this_phase.push(node);
		}
	}

	// Finally, since the phases are found from the output but must be executed
	// from the input(s), reverse them, too.
	std::reverse(phases.begin(), phases.end());
}

void EffectChain::output_dot(const char *filename)
{
	FILE *fp = fopen(filename, "w");
	if (fp == NULL) {
		perror(filename);
		exit(1);
	}

	fprintf(fp, "digraph G {\n");
	for (unsigned i = 0; i < nodes.size(); ++i) {
		fprintf(fp, "  n%ld [label=\"%s\"];\n", (long)nodes[i], nodes[i]->effect->effect_type_id().c_str());
		for (unsigned j = 0; j < nodes[i]->outgoing_links.size(); ++j) {
			std::vector<std::string> labels;

			if (nodes[i]->outgoing_links[j]->effect->needs_texture_bounce()) {
				labels.push_back("needs_bounce");
			}
			if (nodes[i]->effect->changes_output_size()) {
				labels.push_back("resize");
			}

			switch (nodes[i]->output_color_space) {
			case COLORSPACE_REC_601_525:
				labels.push_back("spc[rec601-525]");
				break;
			case COLORSPACE_REC_601_625:
				labels.push_back("spc[rec601-625]");
				break;
			default:
				break;
			}

			switch (nodes[i]->output_gamma_curve) {
			case GAMMA_sRGB:
				labels.push_back("gamma[sRGB]");
				break;
			case GAMMA_REC_601:  // and GAMMA_REC_709
				labels.push_back("gamma[rec601/709]");
				break;
			default:
				break;
			}

			if (labels.empty()) {
				fprintf(fp, "  n%ld -> n%ld;\n", (long)nodes[i], (long)nodes[i]->outgoing_links[j]);
			} else {
				std::string label = labels[0];
				for (unsigned k = 1; k < labels.size(); ++k) {
					label += ", " + labels[k];
				}
				fprintf(fp, "  n%ld -> n%ld [label=\"%s\"];\n", (long)nodes[i], (long)nodes[i]->outgoing_links[j], label.c_str());
			}
		}
	}
	fprintf(fp, "}\n");

	fclose(fp);
}

void EffectChain::find_output_size(Phase *phase)
{
	Node *output_node = phase->effects.back();

	// If the last effect explicitly sets an output size,
	// use that.
	if (output_node->effect->changes_output_size()) {
		output_node->effect->get_output_size(&phase->output_width, &phase->output_height);
		return;
	}

	// If not, look at the input phases, if any. We select the largest one
	// (really assuming they all have the same aspect currently), by pixel count.
	if (!phase->inputs.empty()) {
		unsigned best_width = 0, best_height = 0;
		for (unsigned i = 0; i < phase->inputs.size(); ++i) {
			Node *input = phase->inputs[i];
			assert(input->phase->output_width != 0);
			assert(input->phase->output_height != 0);
			if (input->phase->output_width * input->phase->output_height > best_width * best_height) {
				best_width = input->phase->output_width;
				best_height = input->phase->output_height;
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
	output_dot("final.dot");

	// Find the output effect. This is, simply, one that has no outgoing links.
	// If there are multiple ones, the graph is malformed (we do not support
	// multiple outputs right now).
	std::vector<Node *> output_nodes;
	for (unsigned i = 0; i < nodes.size(); ++i) {
		Node *node = nodes[i];
		if (node->outgoing_links.empty()) {
			output_nodes.push_back(node);
		}
	}
	assert(output_nodes.size() == 1);
	Node *output_node = output_nodes[0];

	// Add normalizers to get the output format right.
	if (output_node->output_color_space != output_format.color_space) {
		ColorSpaceConversionEffect *colorspace_conversion = new ColorSpaceConversionEffect();
		colorspace_conversion->set_int("source_space", output_node->output_color_space);
		colorspace_conversion->set_int("destination_space", output_format.color_space);
		std::vector<Effect *> inputs;
		inputs.push_back(output_node->effect);
		colorspace_conversion->add_self_to_effect_chain(this, inputs);

		assert(node_map.count(colorspace_conversion) != 0);
		output_node = node_map[colorspace_conversion];
		output_node->output_color_space = output_format.color_space;
	}
	if (output_node->output_gamma_curve != output_format.gamma_curve) {
		if (output_node->output_gamma_curve != GAMMA_LINEAR) {
			output_node = normalize_to_linear_gamma(output_node);
		}
		GammaCompressionEffect *gamma_conversion = new GammaCompressionEffect();
		gamma_conversion->set_int("destination_curve", output_format.gamma_curve);
		std::vector<Effect *> inputs;
		inputs.push_back(output_node->effect);
		gamma_conversion->add_self_to_effect_chain(this, inputs);

		assert(node_map.count(gamma_conversion) != 0);
		output_node = node_map[gamma_conversion];
		output_node->output_gamma_curve = output_format.gamma_curve;
	}

	// Construct all needed GLSL programs, starting at the output.
	construct_glsl_programs(output_node);

	// If we have more than one phase, we need intermediate render-to-texture.
	// Construct an FBO, and then as many textures as we need.
	// We choose the simplest option of having one texture per output,
	// since otherwise this turns into an (albeit simple)
	// register allocation problem.
	if (phases.size() > 1) {
		glGenFramebuffers(1, &fbo);

		for (unsigned i = 0; i < phases.size() - 1; ++i) {
			find_output_size(phases[i]);

			Node *output_node = phases[i]->effects.back();
			glGenTextures(1, &output_node->output_texture);
			check_error();
			glBindTexture(GL_TEXTURE_2D, output_node->output_texture);
			check_error();
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			check_error();
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			check_error();
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, phases[i]->output_width, phases[i]->output_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			check_error();

			output_node->output_texture_width = phases[i]->output_width;
			output_node->output_texture_height = phases[i]->output_height;
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

	std::set<Node *> generated_mipmaps;

	for (unsigned phase = 0; phase < phases.size(); ++phase) {
		// See if the requested output size has changed. If so, we need to recreate
		// the texture (and before we start setting up inputs).
		if (phase != phases.size() - 1) {
			find_output_size(phases[phase]);

			Node *output_node = phases[phase]->effects.back();

			if (output_node->output_texture_width != phases[phase]->output_width ||
			    output_node->output_texture_height != phases[phase]->output_height) {
				glActiveTexture(GL_TEXTURE0);
				check_error();
				glBindTexture(GL_TEXTURE_2D, output_node->output_texture);
				check_error();
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, phases[phase]->output_width, phases[phase]->output_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
				check_error();
				glBindTexture(GL_TEXTURE_2D, 0);
				check_error();

				output_node->output_texture_width = phases[phase]->output_width;
				output_node->output_texture_height = phases[phase]->output_height;
			}
		}

		glUseProgram(phases[phase]->glsl_program_num);
		check_error();

		// Set up RTT inputs for this phase.
		for (unsigned sampler = 0; sampler < phases[phase]->inputs.size(); ++sampler) {
			glActiveTexture(GL_TEXTURE0 + sampler);
			Node *input = phases[phase]->inputs[sampler];
			glBindTexture(GL_TEXTURE_2D, input->output_texture);
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

			std::string texture_name = std::string("tex_") + input->effect_id;
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
			Node *output_node = phases[phase]->effects.back();
			glFramebufferTexture2D(
				GL_FRAMEBUFFER,
			        GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_2D,
				output_node->output_texture,
				0);
			check_error();
			glViewport(0, 0, phases[phase]->output_width, phases[phase]->output_height);
		}

		// Give the required parameters to all the effects.
		unsigned sampler_num = phases[phase]->inputs.size();
		for (unsigned i = 0; i < phases[phase]->effects.size(); ++i) {
			Node *node = phases[phase]->effects[i];
			node->effect->set_gl_state(phases[phase]->glsl_program_num, node->effect_id, &sampler_num);
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
			Node *node = phases[phase]->effects[i];
			node->effect->clear_gl_state();
		}
	}
}
