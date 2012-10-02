#include "mirror_effect.h"
#include "util.h"

MirrorEffect::MirrorEffect()
{
}

std::string MirrorEffect::output_fragment_shader()
{
	return read_file("mirror_effect.frag");
}
