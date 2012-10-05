vec4 FUNCNAME(vec2 tc) {
	vec4 first = INPUT1(tc);
	vec4 second = INPUT2(tc);
	return vec4(PREFIX(strength_first)) * first + vec4(PREFIX(strength_second)) * second;
}
