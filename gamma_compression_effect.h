#ifndef _GAMMA_COMPRESSION_EFFECT_H 
#define _GAMMA_COMPRESSION_EFFECT_H 1

// An effect to convert linear light to the given gamma curve,
// typically inserted by the framework automatically at the end
// of the processing chain.
//
// Currently supports sRGB and Rec. 601/709.

#include "effect.h"
#include "effect_chain.h"

#define COMPRESSION_CURVE_SIZE 4096

class GammaCompressionEffect : public Effect {
private:
	// Should not be instantiated by end users.
	GammaCompressionEffect();
	friend class EffectChain;

public:
	virtual std::string effect_type_id() const { return "GammaCompressionEffect"; }
	std::string output_fragment_shader();

	virtual bool needs_srgb_primaries() const { return false; }

	// Actually needs postmultiplied input as well as outputting it.
	// EffectChain will take care of that.
	virtual AlphaHandling alpha_handling() const { return OUTPUT_POSTMULTIPLIED_ALPHA; }

private:
	GammaCurve destination_curve;
	float compression_curve[COMPRESSION_CURVE_SIZE];
};

#endif // !defined(_GAMMA_COMPRESSION_EFFECT_H)
