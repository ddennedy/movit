// See footer.frag for details about this if statement.
#ifndef YCBCR_ALSO_OUTPUT_RGBA
#define YCBCR_ALSO_OUTPUT_RGBA 0
#endif

#if YCBCR_ALSO_OUTPUT_RGBA
vec4[2] FUNCNAME(vec2 tc) {
#else
vec4 FUNCNAME(vec2 tc) {
#endif
	vec4 rgba = INPUT(tc);
	vec4 ycbcr_a;

	ycbcr_a.rgb = PREFIX(ycbcr_matrix) * rgba.rgb + PREFIX(offset);

	if (PREFIX(clamp_range)) {
		// If we use limited-range Y'CbCr, the card's usual 0–255 clamping
		// won't be enough, so we need to clamp ourselves here.
		//
		// We clamp before dither, which is a bit unfortunate, since
		// it means dither can take us out of the clamped range again.
		// However, since DitherEffect never adds enough dither to change
		// the quantized levels, we will be fine in practice.
		ycbcr_a.rgb = clamp(ycbcr_a.rgb, PREFIX(ycbcr_min), PREFIX(ycbcr_max));
	}

	ycbcr_a.a = rgba.a;

#if YCBCR_ALSO_OUTPUT_RGBA
	return vec4[2](ycbcr_a, rgba);
#else
	return ycbcr_a;
#endif
}
