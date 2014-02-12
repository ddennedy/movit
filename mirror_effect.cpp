#include "mirror_effect.h"
#include "util.h"

using namespace std;

namespace movit {

MirrorEffect::MirrorEffect()
{
}

string MirrorEffect::output_fragment_shader()
{
	return read_file("mirror_effect.frag");
}

}  // namespace movit
