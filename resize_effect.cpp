#include "resize_effect.h"
#include "util.h"

using namespace std;

namespace movit {

ResizeEffect::ResizeEffect()
	: width(1280), height(720)
{
	register_int("width", &width);
	register_int("height", &height);
}

string ResizeEffect::output_fragment_shader()
{
	return read_file("identity.frag");
}

void ResizeEffect::get_output_size(unsigned *width, unsigned *height, unsigned *virtual_width, unsigned *virtual_height) const
{
	*virtual_width = *width = this->width;
	*virtual_height = *height = this->height;
}

}  // namespace movit
