uniform sampler2D PREFIX(tex_y);
uniform sampler2D PREFIX(tex_cb);
uniform sampler2D PREFIX(tex_cr);

vec4 FUNCNAME(vec2 tc) {
	vec4 rgba = INPUT(tc);
	vec4 ycbcr_a;

	ycbcr_a.rgb = PREFIX(ycbcr_matrix) * rgba.rgb + PREFIX(offset);

#if YCBCR_CLAMP_RANGE
	// If we use limited-range Y'CbCr, the card's usual 0–255 clamping
	// won't be enough, so we need to clamp ourselves here.
	//
	// We clamp before dither, which is a bit unfortunate, since
	// it means dither can take us out of the clamped range again.
	// However, since DitherEffect never adds enough dither to change
	// the quantized levels, we will be fine in practice.
	ycbcr_a.rgb = clamp(ycbcr_a.rgb, PREFIX(ycbcr_min), PREFIX(ycbcr_max));
#endif

	ycbcr_a.a = rgba.a;
	return ycbcr_a;
}
