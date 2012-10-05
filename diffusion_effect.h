#ifndef _DIFFUSION_EFFECT_H
#define _DIFFUSION_EFFECT_H 1

// There are many different effects that go under the name of "diffusion",
// seemingly all of the inspired by the effect you get when you put a
// diffusion filter in front of your camera lens. The effect most people
// want is a general flattening/smoothing of the light, and reduction of
// fine detail (most notably, blemishes in people's skin), without ruining
// edges, which a regular blur would do.
//
// We do a relatively simple version, sometimes known as "white diffusion",
// where we first blur the picture, and then overlay it on the original
// using the original as a matte.

#include "effect.h"

class BlurEffect;
class OverlayMatteEffect;

class DiffusionEffect : public Effect {
public:
	DiffusionEffect();
	std::string output_fragment_shader();

	virtual bool needs_srgb_primaries() const { return false; }

	virtual void add_self_to_effect_chain(EffectChain *chain, const std::vector<Effect *> &input);
	virtual bool set_float(const std::string &key, float value);

private:
	BlurEffect *blur;
	OverlayMatteEffect *overlay_matte;
};

// Used internally by DiffusionEffect; combines the blurred and the original
// version using the original as a matte.
class OverlayMatteEffect : public Effect {
public:
	OverlayMatteEffect();
	std::string output_fragment_shader();

	unsigned num_inputs() const { return 2; }

private:
	float blurred_mix_amount;
};


#endif // !defined(_DIFFUSION_EFFECT_H)
