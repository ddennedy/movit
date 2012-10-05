vec4 FUNCNAME(vec2 tc) {
	vec4 orig = INPUT1(tc);
	vec4 blurred = INPUT2(tc);
	return mix(orig, blurred, orig * vec4(PREFIX(blurred_mix_amount)));
}
