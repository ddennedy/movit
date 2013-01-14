#include "alpha_multiplication_effect.h"
#include "util.h"

std::string AlphaMultiplicationEffect::output_fragment_shader()
{
	return read_file("alpha_multiplication_effect.frag");
}
