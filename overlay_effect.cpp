#include "overlay_effect.h"
#include "util.h"

using namespace std;

OverlayEffect::OverlayEffect() {}

string OverlayEffect::output_fragment_shader()
{
	return read_file("overlay_effect.frag");
}
