#include <math.h>
#include <assert.h>

#include "diffusion_effect.h"
#include "blur_effect.h"
#include "effect_chain.h"
#include "util.h"

DiffusionEffect::DiffusionEffect()
	: blur(new BlurEffect),
	  overlay_matte(new OverlayMatteEffect)
{
}

std::string DiffusionEffect::output_fragment_shader()
{
	return read_file("sandbox_effect.frag");
}

void DiffusionEffect::add_self_to_effect_chain(EffectChain *chain, const std::vector<Effect *> &inputs) {
	assert(inputs.size() == 1);
	blur->add_self_to_effect_chain(chain, inputs);

	std::vector<Effect *> overlay_matte_inputs;
	overlay_matte_inputs.push_back(inputs[0]);
	overlay_matte_inputs.push_back(chain->last_added_effect());  // FIXME
	overlay_matte->add_self_to_effect_chain(chain, overlay_matte_inputs);
}

bool DiffusionEffect::set_float(const std::string &key, float value) {
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

std::string OverlayMatteEffect::output_fragment_shader()
{
	return read_file("overlay_matte_effect.frag");
}
