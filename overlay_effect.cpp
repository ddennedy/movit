#include "overlay_effect.h"
#include "util.h"

OverlayEffect::OverlayEffect() {}

std::string OverlayEffect::output_fragment_shader()
{
	return read_file("overlay_effect.frag");
}
