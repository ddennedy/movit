#include "resize_effect.h"
#include "util.h"

ResizeEffect::ResizeEffect()
	: width(1280), height(720)
{
	register_int("width", &width);
	register_int("height", &height);
}

std::string ResizeEffect::output_fragment_shader()
{
	return read_file("identity.frag");
}

void ResizeEffect::get_output_size(unsigned *width, unsigned *height) const
{
	*width = this->width;
	*height = this->height;
}
