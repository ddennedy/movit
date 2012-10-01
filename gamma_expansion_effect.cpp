#include "gamma_expansion_effect.h"
#include "util.h"

GammaExpansionEffect::GammaExpansionEffect()
	: source_curve(GAMMA_LINEAR)
{
	register_int("source_curve", (int *)&source_curve);
}

std::string GammaExpansionEffect::output_glsl()
{
	return read_file("todo.glsl");
}
