#include <assert.h>
#include <vector>

#include "blur_effect.h"
#include "diffusion_effect.h"
#include "effect_chain.h"
#include "util.h"

using namespace std;

namespace movit {

DiffusionEffect::DiffusionEffect()
	: blur(new BlurEffect),
	  overlay_matte(new OverlayMatteEffect),
	  owns_overlay_matte(true)
{
}

DiffusionEffect::~DiffusionEffect()
{
	if (owns_overlay_matte) {
		delete overlay_matte;
	}
}

void DiffusionEffect::rewrite_graph(EffectChain *graph, Node *self)
{
	assert(self->incoming_links.size() == 1);
	Node *input = self->incoming_links[0];

	Node *blur_node = graph->add_node(blur);
	Node *overlay_matte_node = graph->add_node(overlay_matte);
	owns_overlay_matte = false;
	graph->replace_receiver(self, overlay_matte_node);
	graph->connect_nodes(input, blur_node);
	graph->connect_nodes(blur_node, overlay_matte_node);
	graph->replace_sender(self, overlay_matte_node);

	self->disabled = true;
}

bool DiffusionEffect::set_float(const string &key, float value) {
	if (key == "blurred_mix_amount") {
		return overlay_matte->set_float(key, value);
	}
	return blur->set_float(key, value);
}

OverlayMatteEffect::OverlayMatteEffect()
	: blurred_mix_amount(0.3f)
{
	register_float("blurred_mix_amount", &blurred_mix_amount);
}

string OverlayMatteEffect::output_fragment_shader()
{
	return read_file("overlay_matte_effect.frag");
}

}  // namespace movit
