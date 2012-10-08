#include <math.h>
#include <assert.h>

#include "glow_effect.h"
#include "blur_effect.h"
#include "mix_effect.h"
#include "effect_chain.h"
#include "util.h"

GlowEffect::GlowEffect()
	: blur(new BlurEffect),
	  mix(new MixEffect)
{
	mix->set_float("strength_first", 1.0f);
	mix->set_float("strength_second", 0.3f);
}

void GlowEffect::rewrite_graph(EffectChain *graph, Node *self)
{
	assert(self->incoming_links.size() == 1);
	Node *input = self->incoming_links[0];

	Node *blur_node = graph->add_node(blur);
	Node *mix_node = graph->add_node(mix);
	graph->replace_receiver(self, mix_node);
	graph->connect_nodes(input, blur_node);
	graph->connect_nodes(blur_node, mix_node);
	graph->replace_sender(self, mix_node);

	self->disabled = true;
}

bool GlowEffect::set_float(const std::string &key, float value) {
	if (key == "blurred_mix_amount") {
		return mix->set_float("strength_second", value);
	}
	return blur->set_float(key, value);
}
