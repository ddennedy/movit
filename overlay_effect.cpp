#include "overlay_effect.h"
#include "util.h"

using namespace std;

namespace movit {

OverlayEffect::OverlayEffect()
	: swap_inputs(false)
{
	register_int("swap_inputs", (int *)&swap_inputs);
}

string OverlayEffect::output_fragment_shader()
{
	char buf[256];
	snprintf(buf, sizeof(buf), "#define SWAP_INPUTS %d\n", swap_inputs);
	return buf + read_file("overlay_effect.frag");
}

}  // namespace movit
