#include "colorspace_conversion_effect.h"
#include "util.h"

ColorSpaceConversionEffect::ColorSpaceConversionEffect()
	: source_space(COLORSPACE_sRGB),
	  destination_space(COLORSPACE_sRGB)
{
	register_int("source_space", (int *)&source_space);
	register_int("destination_space", (int *)&destination_space);
}

std::string ColorSpaceConversionEffect::output_glsl()
{
	return read_file("todo.glsl");
}
