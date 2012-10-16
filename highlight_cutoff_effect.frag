vec4 FUNCNAME(vec2 tc) {
	return max(INPUT(tc) - vec4(PREFIX(cutoff)), 0.0);
}
