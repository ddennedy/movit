#include "colorspace_conversion_effect.h"

ColorSpaceConversionEffect::ColorSpaceConversionEffect()
	: source_space(COLORSPACE_sRGB),
	  destination_space(COLORSPACE_sRGB)
{
	register_int("source_space", (int *)&source_space);
	register_int("destination_space", (int *)&destination_space);
}
