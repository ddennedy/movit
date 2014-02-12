#include "multiply_effect.h"
#include "util.h"

using namespace std;

namespace movit {

MultiplyEffect::MultiplyEffect()
	: factor(1.0f, 1.0f, 1.0f, 1.0f)
{
	register_vec4("factor", (float *)&factor);
}

string MultiplyEffect::output_fragment_shader()
{
	return read_file("multiply_effect.frag");
}

}  // namespace movit
