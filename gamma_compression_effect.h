#ifndef _GAMMA_COMPRESSION_EFFECT_H 
#define _GAMMA_COMPRESSION_EFFECT_H 1

#include "effect.h"
#include "effect_chain.h"

#define COMPRESSION_CURVE_SIZE 4096

class GammaCompressionEffect : public Effect {
public:
	GammaCompressionEffect();
	std::string output_fragment_shader();

	virtual bool needs_srgb_primaries() { return false; }

private:
	GammaCurve destination_curve;
	float compression_curve[COMPRESSION_CURVE_SIZE];
};

#endif // !defined(_GAMMA_COMPRESSION_EFFECT_H)
