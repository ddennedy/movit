#define GL_GLEXT_PROTOTYPES 1

#include <stdio.h>
#include <math.h>
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
#include "dither_effect.h"
#include "input.h"
#include "opengl.h"

EffectChain::EffectChain(float aspect_nom, float aspect_denom)
	: aspect_nom(aspect_nom),
	  aspect_denom(aspect_denom),
	  dither_effect(NULL),
	  fbo(0),
	  num_dither_bits(0),
	  finalized(false) {}

EffectChain::~EffectChain()
{
	for (unsigned i = 0; i < nodes.size(); ++i) {
		if (nodes[i]->output_texture != 0) {
			glDeleteTextures(1, &nodes[i]->output_texture);
		}
		delete nodes[i]->effect;
		delete nodes[i];
	}
	for (unsigned i = 0; i < phases.size(); ++i) {
		glDeleteProgram(phases[i]->glsl_program_num);
		glDeleteShader(phases[i]->vertex_shader);
		glDeleteShader(phases[i]->fragment_shader);
		delete phases[i];
	}
	if (fbo != 0) {
		glDeleteFramebuffers(1, &fbo);
	}
}

Input *EffectChain::add_input(Input *input)
{
	inputs.push_back(input);

	Node *node = add_node(input);
	node->output_color_space = input->get_color_space();
	node->output_gamma_curve = input->get_gamma_curve();
	return input;
}

void EffectChain::add_output(const ImageFormat &format)
{
	output_format = format;
}

Node *EffectChain::add_node(Effect *effect)
{
	char effect_id[256];
	sprintf(effect_id, "eff%u", (unsigned)nodes.size());

	Node *node = new Node;
	node->effect = effect;
	node->disabled = false;
	node->effect_id = effect_id;
	node->output_color_space = COLORSPACE_INVALID;
	node->output_gamma_curve = GAMMA_INVALID;
	node->output_texture = 0;

	nodes.push_back(node);
	node_map[effect] = node;
	return node;
}

void EffectChain::connect_nodes(Node *sender, Node *receiver)
{
	sender->outgoing_links.push_back(receiver);
	receiver->incoming_links.push_back(sender);
}

void EffectChain::replace_receiver(Node *old_receiver, Node *new_receiver)
{
	new_receiver->incoming_links = old_receiver->incoming_links;
	old_receiver->incoming_links.clear();
	
	for (unsigned i = 0; i < new_receiver->incoming_links.size(); ++i) {
		Node *sender = new_receiver->incoming_links[i];
		for (unsigned j = 0; j < sender->outgoing_links.size(); ++j) {
			if (sender->outgoing_links[j] == old_receiver) {
				sender->outgoing_links[j] = new_receiver;
			}
		}
	}	
}

void EffectChain::replace_sender(Node *old_sender, Node *new_sender)
{
	new_sender->outgoing_links = old_sender->outgoing_links;
	old_sender->outgoing_links.clear();
	
	for (unsigned i = 0; i < new_sender->outgoing_links.size(); ++i) {
		Node *receiver = new_sender->outgoing_links[i];
		for (unsigned j = 0; j < receiver->incoming_links.size(); ++j) {
			if (receiver->incoming_links[j] == old_sender) {
				receiver->incoming_links[j] = new_sender;
			}
		}
	}	
}

void EffectChain::insert_node_between(Node *sender, Node *middle, Node *receiver)
{
	for (unsigned i = 0; i < sender->outgoing_links.size(); ++i) {
		if (sender->outgoing_links[i] == receiver) {
			sender->outgoing_links[i] = middle;
			middle->incoming_links.push_back(sender);
		}
	}
	for (unsigned i = 0; i < receiver->incoming_links.size(); ++i) {
		if (receiver->incoming_links[i] == sender) {
			receiver->incoming_links[i] = middle;
			middle->outgoing_links.push_back(receiver);
		}
	}

	assert(middle->incoming_links.size() == middle->effect->num_inputs());
}

void EffectChain::find_all_nonlinear_inputs(Node *node, std::vector<Node *> *nonlinear_inputs)
{
	if (node->output_gamma_curve == GAMMA_LINEAR &&
	    node->effect->effect_type_id() != "GammaCompressionEffect") {
		return;
	}
	if (node->effect->num_inputs() == 0) {
		nonlinear_inputs->push_back(node);
	} else {
		assert(node->effect->num_inputs() == node->incoming_links.size());
		for (unsigned i = 0; i < node->incoming_links.size(); ++i) {
			find_all_nonlinear_inputs(node->incoming_links[i], nonlinear_inputs);
		}
	}
}

Effect *EffectChain::add_effect(Effect *effect, const std::vector<Effect *> &inputs)
{
	assert(inputs.size() == effect->num_inputs());
	Node *node = add_node(effect);
	for (unsigned i = 0; i < inputs.size(); ++i) {
		assert(node_map.count(inputs[i]) != 0);
		connect_nodes(node_map[inputs[i]], node);
	}
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
			CHECK(node->effect->set_int("needs_mipmaps", input_needs_mipmaps));
		}
	}
	frag_shader += std::string("#define INPUT ") + effects.back()->effect_id + "\n";
	frag_shader.append(read_file("footer.frag"));

	// Output shader to a temporary file, for easier debugging.
	static int compiled_shader_num = 0;
	char filename[256];
	sprintf(filename, "chain-%03d.frag", compiled_shader_num++);
	FILE *fp = fopen(filename, "w");
	if (fp == NULL) {
		perror(filename);
		exit(1);
	}
	fprintf(fp, "%s\n", frag_shader.c_str());
	fclose(fp);
	
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
	phase->vertex_shader = vs_obj;
	phase->fragment_shader = fs_obj;
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

				if (deps[i]->outgoing_links.size() > 1) {
					if (deps[i]->effect->num_inputs() > 0) {
						// More than one effect uses this as the input,
						// and it is not a texture itself.
						// The easiest thing to do (and probably also the safest
						// performance-wise in most cases) is to bounce it to a texture
						// and then let the next passes read from that.
						start_new_phase = true;
					} else {
						// For textures, we try to be slightly more clever;
						// if none of our outputs need a bounce, we don't bounce
						// but instead simply use the effect many times.
						//
						// Strictly speaking, we could bounce it for some outputs
						// and use it directly for others, but the processing becomes
						// somewhat simpler if the effect is only used in one such way.
						for (unsigned j = 0; j < deps[i]->outgoing_links.size(); ++j) {
							Node *rdep = deps[i]->outgoing_links[j];
							start_new_phase |= rdep->effect->needs_texture_bounce();
						}
					}
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
		// Find out which phase this event belongs to.
		int in_phase = -1;
		for (unsigned j = 0; j < phases.size(); ++j) {
			const Phase* p = phases[j];
			if (std::find(p->effects.begin(), p->effects.end(), nodes[i]) != p->effects.end()) {
				assert(in_phase == -1);
				in_phase = j;
			}
		}

		if (in_phase == -1) {
			fprintf(fp, "  n%ld [label=\"%s\"];\n", (long)nodes[i], nodes[i]->effect->effect_type_id().c_str());
		} else {
			fprintf(fp, "  n%ld [label=\"%s\" style=\"filled\" fillcolor=\"/accent8/%d\"];\n",
				(long)nodes[i], nodes[i]->effect->effect_type_id().c_str(),
				(in_phase % 8) + 1);
		}
		for (unsigned j = 0; j < nodes[i]->outgoing_links.size(); ++j) {
			std::vector<std::string> labels;

			if (nodes[i]->outgoing_links[j]->effect->needs_texture_bounce()) {
				labels.push_back("needs_bounce");
			}
			if (nodes[i]->effect->changes_output_size()) {
				labels.push_back("resize");
			}

			switch (nodes[i]->output_color_space) {
			case COLORSPACE_INVALID:
				labels.push_back("spc[invalid]");
				break;
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
			case GAMMA_INVALID:
				labels.push_back("gamma[invalid]");
				break;
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

unsigned EffectChain::fit_rectangle_to_aspect(unsigned width, unsigned height)
{
	if (float(width) * aspect_denom >= float(height) * aspect_nom) {
		// Same aspect, or W/H > aspect (image is wider than the frame).
		// In either case, keep width.
		return width;
	} else {
		// W/H < aspect (image is taller than the frame), so keep height,
		// and adjust width correspondingly.
		return lrintf(height * aspect_nom / aspect_denom);
	}
}

// Propagate input texture sizes throughout, and inform effects downstream.
// (Like a lot of other code, we depend on effects being in topological order.)
void EffectChain::inform_input_sizes(Phase *phase)
{
	// All effects that have a defined size (inputs and RTT inputs)
	// get that. Reset all others.
	for (unsigned i = 0; i < phase->effects.size(); ++i) {
		Node *node = phase->effects[i];
		if (node->effect->num_inputs() == 0) {
			Input *input = static_cast<Input *>(node->effect);
			node->output_width = input->get_width();
			node->output_height = input->get_height();
			assert(node->output_width != 0);
			assert(node->output_height != 0);
		} else {
			node->output_width = node->output_height = 0;
		}
	}
	for (unsigned i = 0; i < phase->inputs.size(); ++i) {
		Node *input = phase->inputs[i];
		input->output_width = input->phase->output_width;
		input->output_height = input->phase->output_height;
		assert(input->output_width != 0);
		assert(input->output_height != 0);
	}

	// Now propagate from the inputs towards the end, and inform as we go.
	// The rules are simple:
	//
	//   1. Don't touch effects that already have given sizes (ie., inputs).
	//   2. If all of your inputs have the same size, that will be your output size.
	//   3. Otherwise, your output size is 0x0.
	for (unsigned i = 0; i < phase->effects.size(); ++i) {
		Node *node = phase->effects[i];
		if (node->effect->num_inputs() == 0) {
			continue;
		}
		unsigned this_output_width = 0;
		unsigned this_output_height = 0;
		for (unsigned j = 0; j < node->incoming_links.size(); ++j) {
			Node *input = node->incoming_links[j];
			node->effect->inform_input_size(j, input->output_width, input->output_height);
			if (j == 0) {
				this_output_width = input->output_width;
				this_output_height = input->output_height;
			} else if (input->output_width != this_output_width || input->output_height != this_output_height) {
				// Inputs disagree.
				this_output_width = 0;
				this_output_height = 0;
			}
		}
		node->output_width = this_output_width;
		node->output_height = this_output_height;
	}
}

// Note: You should call inform_input_sizes() before this, as the last effect's
// desired output size might change based on the inputs.
void EffectChain::find_output_size(Phase *phase)
{
	Node *output_node = phase->effects.back();

	// If the last effect explicitly sets an output size, use that.
	if (output_node->effect->changes_output_size()) {
		output_node->effect->get_output_size(&phase->output_width, &phase->output_height);
		return;
	}

	// If not, look at the input phases and textures.
	// We select the largest one (by fit into the current aspect).
	unsigned best_width = 0;
	for (unsigned i = 0; i < phase->inputs.size(); ++i) {
		Node *input = phase->inputs[i];
		assert(input->phase->output_width != 0);
		assert(input->phase->output_height != 0);
		unsigned width = fit_rectangle_to_aspect(input->phase->output_width, input->phase->output_height);
		if (width > best_width) {
			best_width = width;
		}
	}
	for (unsigned i = 0; i < phase->effects.size(); ++i) {
		Effect *effect = phase->effects[i]->effect;
		if (effect->num_inputs() != 0) {
			continue;
		}

		Input *input = static_cast<Input *>(effect);
		unsigned width = fit_rectangle_to_aspect(input->get_width(), input->get_height());
		if (width > best_width) {
			best_width = width;
		}
	}
	assert(best_width != 0);
	phase->output_width = best_width;
	phase->output_height = best_width * aspect_denom / aspect_nom;
}

void EffectChain::sort_nodes_topologically()
{
	std::set<Node *> visited_nodes;
	std::vector<Node *> sorted_list;
	for (unsigned i = 0; i < nodes.size(); ++i) {
		if (nodes[i]->incoming_links.size() == 0) {
			topological_sort_visit_node(nodes[i], &visited_nodes, &sorted_list);
		}
	}
	reverse(sorted_list.begin(), sorted_list.end());
	nodes = sorted_list;
}

void EffectChain::topological_sort_visit_node(Node *node, std::set<Node *> *visited_nodes, std::vector<Node *> *sorted_list)
{
	if (visited_nodes->count(node) != 0) {
		return;
	}
	visited_nodes->insert(node);
	for (unsigned i = 0; i < node->outgoing_links.size(); ++i) {
		topological_sort_visit_node(node->outgoing_links[i], visited_nodes, sorted_list);
	}
	sorted_list->push_back(node);
}

// Propagate gamma and color space information as far as we can in the graph.
// The rules are simple: Anything where all the inputs agree, get that as
// output as well. Anything else keeps having *_INVALID.
void EffectChain::propagate_gamma_and_color_space()
{
	// We depend on going through the nodes in order.
	sort_nodes_topologically();

	for (unsigned i = 0; i < nodes.size(); ++i) {
		Node *node = nodes[i];
		if (node->disabled) {
			continue;
		}
		assert(node->incoming_links.size() == node->effect->num_inputs());
		if (node->incoming_links.size() == 0) {
			assert(node->output_color_space != COLORSPACE_INVALID);
			assert(node->output_gamma_curve != GAMMA_INVALID);
			continue;
		}

		Colorspace color_space = node->incoming_links[0]->output_color_space;
		GammaCurve gamma_curve = node->incoming_links[0]->output_gamma_curve;
		for (unsigned j = 1; j < node->incoming_links.size(); ++j) {
			if (node->incoming_links[j]->output_color_space != color_space) {
				color_space = COLORSPACE_INVALID;
			}
			if (node->incoming_links[j]->output_gamma_curve != gamma_curve) {
				gamma_curve = GAMMA_INVALID;
			}
		}

		// The conversion effects already have their outputs set correctly,
		// so leave them alone.
		if (node->effect->effect_type_id() != "ColorspaceConversionEffect") {
			node->output_color_space = color_space;
		}		
		if (node->effect->effect_type_id() != "GammaCompressionEffect" &&
		    node->effect->effect_type_id() != "GammaExpansionEffect") {
			node->output_gamma_curve = gamma_curve;
		}		
	}
}

bool EffectChain::node_needs_colorspace_fix(Node *node)
{
	if (node->disabled) {
		return false;
	}
	if (node->effect->num_inputs() == 0) {
		return false;
	}

	// propagate_gamma_and_color_space() has already set our output
	// to COLORSPACE_INVALID if the inputs differ, so we can rely on that.
	if (node->output_color_space == COLORSPACE_INVALID) {
		return true;
	}
	return (node->effect->needs_srgb_primaries() && node->output_color_space != COLORSPACE_sRGB);
}

// Fix up color spaces so that there are no COLORSPACE_INVALID nodes left in
// the graph. Our strategy is not always optimal, but quite simple:
// Find an effect that's as early as possible where the inputs are of
// unacceptable colorspaces (that is, either different, or, if the effect only
// wants sRGB, not sRGB.) Add appropriate conversions on all its inputs,
// propagate the information anew, and repeat until there are no more such
// effects.
void EffectChain::fix_internal_color_spaces()
{
	unsigned colorspace_propagation_pass = 0;
	bool found_any;
	do {
		found_any = false;
		for (unsigned i = 0; i < nodes.size(); ++i) {
			Node *node = nodes[i];
			if (!node_needs_colorspace_fix(node)) {
				continue;
			}

			// Go through each input that is not sRGB, and insert
			// a colorspace conversion before it.
			for (unsigned j = 0; j < node->incoming_links.size(); ++j) {
				Node *input = node->incoming_links[j];
				assert(input->output_color_space != COLORSPACE_INVALID);
				if (input->output_color_space == COLORSPACE_sRGB) {
					continue;
				}
				Node *conversion = add_node(new ColorspaceConversionEffect());
				CHECK(conversion->effect->set_int("source_space", input->output_color_space));
				CHECK(conversion->effect->set_int("destination_space", COLORSPACE_sRGB));
				conversion->output_color_space = COLORSPACE_sRGB;
				insert_node_between(input, conversion, node);
			}

			// Re-sort topologically, and propagate the new information.
			propagate_gamma_and_color_space();
			
			found_any = true;
			break;
		}
	
		char filename[256];
		sprintf(filename, "step3-colorspacefix-iter%u.dot", ++colorspace_propagation_pass);
		output_dot(filename);
		assert(colorspace_propagation_pass < 100);
	} while (found_any);

	for (unsigned i = 0; i < nodes.size(); ++i) {
		Node *node = nodes[i];
		if (node->disabled) {
			continue;
		}
		assert(node->output_color_space != COLORSPACE_INVALID);
	}
}

// Make so that the output is in the desired color space.
void EffectChain::fix_output_color_space()
{
	Node *output = find_output_node();
	if (output->output_color_space != output_format.color_space) {
		Node *conversion = add_node(new ColorspaceConversionEffect());
		CHECK(conversion->effect->set_int("source_space", output->output_color_space));
		CHECK(conversion->effect->set_int("destination_space", output_format.color_space));
		conversion->output_color_space = output_format.color_space;
		connect_nodes(output, conversion);
		propagate_gamma_and_color_space();
	}
}

bool EffectChain::node_needs_gamma_fix(Node *node)
{
	if (node->disabled) {
		return false;
	}

	// Small hack since the output is not an explicit node:
	// If we are the last node and our output is in the wrong
	// space compared to EffectChain's output, we need to fix it.
	// This will only take us to linear, but fix_output_gamma()
	// will come and take us to the desired output gamma
	// if it is needed.
	//
	// This needs to be before everything else, since it could
	// even apply to inputs (if they are the only effect).
	if (node->outgoing_links.empty() &&
	    node->output_gamma_curve != output_format.gamma_curve &&
	    node->output_gamma_curve != GAMMA_LINEAR) {
		return true;
	}

	if (node->effect->num_inputs() == 0) {
		return false;
	}

	// propagate_gamma_and_color_space() has already set our output
	// to GAMMA_INVALID if the inputs differ, so we can rely on that,
	// except for GammaCompressionEffect.
	if (node->output_gamma_curve == GAMMA_INVALID) {
		return true;
	}
	if (node->effect->effect_type_id() == "GammaCompressionEffect") {
		assert(node->incoming_links.size() == 1);
		return node->incoming_links[0]->output_gamma_curve != GAMMA_LINEAR;
	}

	return (node->effect->needs_linear_light() && node->output_gamma_curve != GAMMA_LINEAR);
}

// Very similar to fix_internal_color_spaces(), but for gamma.
// There is one difference, though; before we start adding conversion nodes,
// we see if we can get anything out of asking the sources to deliver
// linear gamma directly. fix_internal_gamma_by_asking_inputs()
// does that part, while fix_internal_gamma_by_inserting_nodes()
// inserts nodes as needed afterwards.
void EffectChain::fix_internal_gamma_by_asking_inputs(unsigned step)
{
	unsigned gamma_propagation_pass = 0;
	bool found_any;
	do {
		found_any = false;
		for (unsigned i = 0; i < nodes.size(); ++i) {
			Node *node = nodes[i];
			if (!node_needs_gamma_fix(node)) {
				continue;
			}

			// See if all inputs can give us linear gamma. If not, leave it.
			std::vector<Node *> nonlinear_inputs;
			find_all_nonlinear_inputs(node, &nonlinear_inputs);
			assert(!nonlinear_inputs.empty());

			bool all_ok = true;
			for (unsigned i = 0; i < nonlinear_inputs.size(); ++i) {
				Input *input = static_cast<Input *>(nonlinear_inputs[i]->effect);
				all_ok &= input->can_output_linear_gamma();
			}

			if (!all_ok) {
				continue;
			}

			for (unsigned i = 0; i < nonlinear_inputs.size(); ++i) {
				CHECK(nonlinear_inputs[i]->effect->set_int("output_linear_gamma", 1));
				nonlinear_inputs[i]->output_gamma_curve = GAMMA_LINEAR;
			}

			// Re-sort topologically, and propagate the new information.
			propagate_gamma_and_color_space();
			
			found_any = true;
			break;
		}
	
		char filename[256];
		sprintf(filename, "step%u-gammafix-iter%u.dot", step, ++gamma_propagation_pass);
		output_dot(filename);
		assert(gamma_propagation_pass < 100);
	} while (found_any);
}

void EffectChain::fix_internal_gamma_by_inserting_nodes(unsigned step)
{
	unsigned gamma_propagation_pass = 0;
	bool found_any;
	do {
		found_any = false;
		for (unsigned i = 0; i < nodes.size(); ++i) {
			Node *node = nodes[i];
			if (!node_needs_gamma_fix(node)) {
				continue;
			}

			// Special case: We could be an input and still be asked to
			// fix our gamma; if so, we should be the only node
			// (as node_needs_gamma_fix() would only return true in
			// for an input in that case). That means we should insert
			// a conversion node _after_ ourselves.
			if (node->incoming_links.empty()) {
				assert(node->outgoing_links.empty());
				Node *conversion = add_node(new GammaExpansionEffect());
				CHECK(conversion->effect->set_int("source_curve", node->output_gamma_curve));
				conversion->output_gamma_curve = GAMMA_LINEAR;
				connect_nodes(node, conversion);
			}

			// If not, go through each input that is not linear gamma,
			// and insert a gamma conversion before it.
			for (unsigned j = 0; j < node->incoming_links.size(); ++j) {
				Node *input = node->incoming_links[j];
				assert(input->output_gamma_curve != GAMMA_INVALID);
				if (input->output_gamma_curve == GAMMA_LINEAR) {
					continue;
				}
				Node *conversion = add_node(new GammaExpansionEffect());
				CHECK(conversion->effect->set_int("source_curve", input->output_gamma_curve));
				conversion->output_gamma_curve = GAMMA_LINEAR;
				insert_node_between(input, conversion, node);
			}

			// Re-sort topologically, and propagate the new information.
			propagate_gamma_and_color_space();
			
			found_any = true;
			break;
		}
	
		char filename[256];
		sprintf(filename, "step%u-gammafix-iter%u.dot", step, ++gamma_propagation_pass);
		output_dot(filename);
		assert(gamma_propagation_pass < 100);
	} while (found_any);

	for (unsigned i = 0; i < nodes.size(); ++i) {
		Node *node = nodes[i];
		if (node->disabled) {
			continue;
		}
		assert(node->output_gamma_curve != GAMMA_INVALID);
	}
}

// Make so that the output is in the desired gamma.
// Note that this assumes linear input gamma, so it might create the need
// for another pass of fix_internal_gamma().
void EffectChain::fix_output_gamma()
{
	Node *output = find_output_node();
	if (output->output_gamma_curve != output_format.gamma_curve) {
		Node *conversion = add_node(new GammaCompressionEffect());
		CHECK(conversion->effect->set_int("destination_curve", output_format.gamma_curve));
		conversion->output_gamma_curve = output_format.gamma_curve;
		connect_nodes(output, conversion);
	}
}
	
// If the user has requested dither, add a DitherEffect right at the end
// (after GammaCompressionEffect etc.). This needs to be done after everything else,
// since dither is about the only effect that can _not_ be done in linear space.
void EffectChain::add_dither_if_needed()
{
	if (num_dither_bits == 0) {
		return;
	}
	Node *output = find_output_node();
	Node *dither = add_node(new DitherEffect());
	CHECK(dither->effect->set_int("num_bits", num_dither_bits));
	connect_nodes(output, dither);

	dither_effect = dither->effect;
}

// Find the output node. This is, simply, one that has no outgoing links.
// If there are multiple ones, the graph is malformed (we do not support
// multiple outputs right now).
Node *EffectChain::find_output_node()
{
	std::vector<Node *> output_nodes;
	for (unsigned i = 0; i < nodes.size(); ++i) {
		Node *node = nodes[i];
		if (node->disabled) {
			continue;
		}
		if (node->outgoing_links.empty()) {
			output_nodes.push_back(node);
		}
	}
	assert(output_nodes.size() == 1);
	return output_nodes[0];
}

void EffectChain::finalize()
{
	// Output the graph as it is before we do any conversions on it.
	output_dot("step0-start.dot");

	// Give each effect in turn a chance to rewrite its own part of the graph.
	// Note that if more effects are added as part of this, they will be
	// picked up as part of the same for loop, since they are added at the end.
	for (unsigned i = 0; i < nodes.size(); ++i) {
		nodes[i]->effect->rewrite_graph(this, nodes[i]);
	}
	output_dot("step1-rewritten.dot");

	propagate_gamma_and_color_space();
	output_dot("step2-propagated.dot");

	fix_internal_color_spaces();
	fix_output_color_space();
	output_dot("step4-output-colorspacefix.dot");

	// Note that we need to fix gamma after colorspace conversion,
	// because colorspace conversions might create needs for gamma conversions.
	// Also, we need to run an extra pass of fix_internal_gamma() after 
	// fixing the output gamma, as we only have conversions to/from linear.
	fix_internal_gamma_by_asking_inputs(5);
	fix_internal_gamma_by_inserting_nodes(6);
	fix_output_gamma();
	output_dot("step7-output-gammafix.dot");
	fix_internal_gamma_by_asking_inputs(8);
	fix_internal_gamma_by_inserting_nodes(9);

	output_dot("step10-before-dither.dot");

	add_dither_if_needed();

	output_dot("step11-final.dot");
	
	// Construct all needed GLSL programs, starting at the output.
	construct_glsl_programs(find_output_node());

	output_dot("step12-split-to-phases.dot");

	// If we have more than one phase, we need intermediate render-to-texture.
	// Construct an FBO, and then as many textures as we need.
	// We choose the simplest option of having one texture per output,
	// since otherwise this turns into an (albeit simple)
	// register allocation problem.
	if (phases.size() > 1) {
		glGenFramebuffers(1, &fbo);

		for (unsigned i = 0; i < phases.size() - 1; ++i) {
			inform_input_sizes(phases[i]);
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
		inform_input_sizes(phases.back());
	}
		
	for (unsigned i = 0; i < inputs.size(); ++i) {
		inputs[i]->finalize();
	}

	assert(phases[0]->inputs.empty());
	
	finalized = true;
}

void EffectChain::render_to_fbo(GLuint dest_fbo, unsigned width, unsigned height)
{
	assert(finalized);

	// Save original viewport.
	GLuint x = 0, y = 0;

	if (width == 0 && height == 0) {
		GLint viewport[4];
		glGetIntegerv(GL_VIEWPORT, viewport);
		x = viewport[0];
		y = viewport[1];
		width = viewport[2];
		height = viewport[3];
	}

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
		inform_input_sizes(phases[phase]);
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
			// Last phase goes to the output the user specified.
			glBindFramebuffer(GL_FRAMEBUFFER, dest_fbo);
			check_error();
			glViewport(x, y, width, height);
			if (dither_effect != NULL) {
				CHECK(dither_effect->set_int("output_width", width));
				CHECK(dither_effect->set_int("output_height", height));
			}
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
