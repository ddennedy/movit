// Implicit uniforms:
// uniform sampler2D PREFIX(dither_tex);
// uniform vec2 PREFIX(tc_scale);
// uniform float PREFIX(round_fac), PREFIX(inv_round_fac);

// See footer.frag for details about this if statement.
#ifndef YCBCR_ALSO_OUTPUT_RGBA
#define YCBCR_ALSO_OUTPUT_RGBA 0
#endif

#if YCBCR_ALSO_OUTPUT_RGBA

// There are two values to dither; otherwise, exactly the same as the algorithm below
// (so comments are not duplicated).

vec4[2] FUNCNAME(vec2 tc) {
	vec4[2] result = INPUT(tc);
	float d = tex2D(PREFIX(dither_tex), tc * PREFIX(tc_scale)).x;
	result[0].rgb += vec3(d);
	result[1].rgb += vec3(d);

#if NEED_EXPLICIT_ROUND
	result[0] = round(result[0] * vec4(PREFIX(round_fac))) * vec4(PREFIX(inv_round_fac));
	result[1] = round(result[1] * vec4(PREFIX(round_fac))) * vec4(PREFIX(inv_round_fac));
#endif

	return result;
}

#else

vec4 FUNCNAME(vec2 tc) {
	vec4 result = INPUT(tc);
	float d = tex2D(PREFIX(dither_tex), tc * PREFIX(tc_scale)).x;

	// Don't dither alpha; the case of alpha=255 (1.0) is very important to us,
	// and if there's any inaccuracy earlier in the chain so that it becomes e.g.
	// 254.8, it's better to just get it rounded off than to dither and have it
	// possibly get down to 254. This is not the case for the color components.
	result.rgb += vec3(d);

	// NEED_EXPLICIT_ROUND will be #defined to 1 if the GPU has inaccurate
	// fp32 -> int8 framebuffer rounding, and 0 otherwise.
#if NEED_EXPLICIT_ROUND
	result = round(result * vec4(PREFIX(round_fac))) * vec4(PREFIX(inv_round_fac));
#endif

	return result;
}

#endif
