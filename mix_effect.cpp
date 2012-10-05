#include "mix_effect.h"
#include "util.h"

MixEffect::MixEffect()
	: strength_first(0.5f), strength_second(0.5f)
{
	register_float("strength_first", &strength_first);
	register_float("strength_second", &strength_second);
}

std::string MixEffect::output_fragment_shader()
{
	return read_file("mix_effect.frag");
}
