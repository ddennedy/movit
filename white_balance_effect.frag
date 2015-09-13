// Implicit uniforms:
// uniform mat3 PREFIX(correction_matrix);

vec4 FUNCNAME(vec2 tc) {
	vec4 ret = INPUT(tc);
	ret.rgb = PREFIX(correction_matrix) * ret.rgb;
	return ret;
}
