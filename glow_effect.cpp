#include <assert.h>
#include <vector>

#include "blur_effect.h"
#include "effect_chain.h"
#include "glow_effect.h"
#include "mix_effect.h"
#include "util.h"

using namespace std;

namespace movit {

GlowEffect::GlowEffect()
	: blur(new BlurEffect),
	  cutoff(new HighlightCutoffEffect),
	  mix(new MixEffect)
{
	CHECK(blur->set_float("radius", 20.0f));
	CHECK(mix->set_float("strength_first", 1.0f));
	CHECK(mix->set_float("strength_second", 1.0f));
	CHECK(cutoff->set_float("cutoff", 0.2f));
}

void GlowEffect::rewrite_graph(EffectChain *graph, Node *self)
{
	assert(self->incoming_links.size() == 1);
	Node *input = self->incoming_links[0];

	Node *blur_node = graph->add_node(blur);
	Node *mix_node = graph->add_node(mix);
	Node *cutoff_node = graph->add_node(cutoff);
	graph->replace_receiver(self, mix_node);
	graph->connect_nodes(input, cutoff_node);
	graph->connect_nodes(cutoff_node, blur_node);
	graph->connect_nodes(blur_node, mix_node);
	graph->replace_sender(self, mix_node);

	self->disabled = true;
}

bool GlowEffect::set_float(const string &key, float value) {
	if (key == "blurred_mix_amount") {
		return mix->set_float("strength_second", value);
	}
	if (key == "highlight_cutoff") {
		return cutoff->set_float("cutoff", value);
	}
	return blur->set_float(key, value);
}

HighlightCutoffEffect::HighlightCutoffEffect()
	: cutoff(0.0f)
{
	register_float("cutoff", &cutoff);
}

string HighlightCutoffEffect::output_fragment_shader()
{
	return read_file("highlight_cutoff_effect.frag");
}

}  // namespace movit
