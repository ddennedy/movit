uniform sampler2D PREFIX(dither_tex);
uniform vec2 PREFIX(tc_scale);
uniform float PREFIX(round_fac), PREFIX(inv_round_fac);

vec4 FUNCNAME(vec2 tc) {
	// We also choose to dither alpha, just in case.
	// Maybe it should in theory have a separate dither,
	// but I doubt it matters much.
	vec4 result = INPUT(tc) + texture2D(PREFIX(dither_tex), tc * PREFIX(tc_scale)).xxxx;

	// NEED_EXPLICIT_ROUND will be #defined to 1 if the GPU has inaccurate
	// fp32 -> int8 framebuffer rounding, and 0 otherwise.
#if NEED_EXPLICIT_ROUND
	result = round(result * vec4(PREFIX(round_fac))) * vec4(PREFIX(inv_round_fac));
#endif

	return result;
}
