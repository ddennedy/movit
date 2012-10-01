#include "gamma_expansion_effect.h"

GammaExpansionEffect::GammaExpansionEffect()
	: source_curve(GAMMA_LINEAR)
{
	register_int("source_curve", (int *)&source_curve);
}
