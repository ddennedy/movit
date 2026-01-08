#include "overlay_effect.h"
#include "util.h"

using namespace std;

namespace movit {

OverlayEffect::OverlayEffect()
	: swap_inputs(false),
      blend_mode(BLEND_MODE_SOURCE_OVER)
{
	register_int("swap_inputs", (int *)&swap_inputs);
	register_int("blend_mode", &blend_mode);
}

string OverlayEffect::output_fragment_shader()
{
	char buf[256];
	snprintf(buf, sizeof(buf), "#define SWAP_INPUTS %d\n#", swap_inputs);
	return buf + read_file("overlay_effect.frag");
}

}  // namespace movit
