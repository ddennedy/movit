#include "saturation_effect.h"
#include "util.h"

using namespace std;

namespace movit {

SaturationEffect::SaturationEffect()
	: saturation(1.0f)
{
	register_float("saturation", &saturation);
}

string SaturationEffect::output_fragment_shader()
{
	return read_file("saturation_effect.frag");
}

}  // namespace movit
