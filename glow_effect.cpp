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

void GlowEffect::add_self_to_effect_chain(EffectChain *chain, const std::vector<Effect *> &inputs) {
	assert(inputs.size() == 1);
	blur->add_self_to_effect_chain(chain, inputs);

	std::vector<Effect *> mix_inputs;
	mix_inputs.push_back(inputs[0]);
	mix_inputs.push_back(chain->last_added_effect());  // FIXME
	mix->add_self_to_effect_chain(chain, mix_inputs);
}

bool GlowEffect::set_float(const std::string &key, float value) {
	if (key == "blurred_mix_amount") {
		return mix->set_float("strength_second", value);
	}
	return blur->set_float(key, value);
}
