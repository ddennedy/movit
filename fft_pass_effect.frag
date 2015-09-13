// DIRECTION_VERTICAL will be #defined to 1 if we are doing a vertical FFT,
// and 0 otherwise.

// Implicit uniforms:
// uniform float PREFIX(num_repeats);
// uniform sampler2D PREFIX(support_tex);

vec4 FUNCNAME(vec2 tc) {
#if DIRECTION_VERTICAL
	vec4 support = tex2D(PREFIX(support_tex), vec2(tc.y * PREFIX(num_repeats), 0.0));
        vec4 c1 = INPUT(vec2(tc.x, tc.y + support.x));
        vec4 c2 = INPUT(vec2(tc.x, tc.y + support.y));
#else
	vec4 support = tex2D(PREFIX(support_tex), vec2(tc.x * PREFIX(num_repeats), 0.0));
        vec4 c1 = INPUT(vec2(tc.x + support.x, tc.y));
        vec4 c2 = INPUT(vec2(tc.x + support.y, tc.y));
#endif
	// Two complex additions and multiplications in parallel; essentially
	//
	//   result.xy = c1.xy + twiddle * c2.xy
	//   result.zw = c1.zw + twiddle * c2.zw
	//
	// where * is complex multiplication.
	return c1 + support.z * c2 + support.w * vec4(-c2.y, c2.x, -c2.w, c2.z);
}

#undef DIRECTION_VERTICAL
