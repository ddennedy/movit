#include <GL/glew.h>

#include "multiply_effect.h"
#include "util.h"

MultiplyEffect::MultiplyEffect()
	: factor(1.0f, 1.0f, 1.0f, 1.0f)
{
	register_vec4("factor", (float *)&factor);
}

std::string MultiplyEffect::output_fragment_shader()
{
	return read_file("multiply_effect.frag");
}
