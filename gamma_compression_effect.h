#ifndef _GAMMA_COMPRESSION_EFFECT_H 
#define _GAMMA_COMPRESSION_EFFECT_H 1

#include "effect.h"
#include "effect_chain.h"

class GammaCompressionEffect : public Effect {
public:
	GammaCompressionEffect();
	std::string output_glsl();

private:
	GammaCurve destination_curve;
};

#endif // !defined(_GAMMA_COMPRESSION_EFFECT_H)
