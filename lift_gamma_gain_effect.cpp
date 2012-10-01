#include "lift_gamma_gain_effect.h"

LiftGammaGainEffect::LiftGammaGainEffect()
	: lift(0.0f, 0.0f, 0.0f),
	  gamma(1.0f, 1.0f, 1.0f),
	  gain(1.0f, 1.0f, 1.0f),
	  saturation(1.0f)
{
	register_vec3("lift", (float *)&lift);
	register_vec3("gamma", (float *)&gamma);
	register_vec3("gain", (float *)&gain);
	register_float("saturation", &saturation);
}
