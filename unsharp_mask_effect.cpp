#include <assert.h>
#include <vector>

#include "blur_effect.h"
#include "effect_chain.h"
#include "mix_effect.h"
#include "unsharp_mask_effect.h"
#include "util.h"

using namespace std;

namespace movit {

UnsharpMaskEffect::UnsharpMaskEffect()
	: blur(new BlurEffect),
	  mix(new MixEffect)
{
	CHECK(mix->set_float("strength_first", 1.0f));
	CHECK(mix->set_float("strength_second", -0.3f));
}

void UnsharpMaskEffect::rewrite_graph(EffectChain *graph, Node *self)
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

bool UnsharpMaskEffect::set_float(const string &key, float value) {
	if (key == "amount") {
		bool ok = mix->set_float("strength_first", 1.0f + value);
		return ok && mix->set_float("strength_second", -value);
	}
	return blur->set_float(key, value);
}

}  // namespace movit
