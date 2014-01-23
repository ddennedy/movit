#include "alpha_division_effect.h"
#include "util.h"

using namespace std;

string AlphaDivisionEffect::output_fragment_shader()
{
	return read_file("alpha_division_effect.frag");
}
