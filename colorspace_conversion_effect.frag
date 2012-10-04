// Colorspace conversion (needs to be done in linear space).
// The matrix is computed on the host and baked into the shader at compile time.

vec4 FUNCNAME(vec2 tc) {
	vec4 x = INPUT(tc);
	x.rgb = PREFIX(conversion_matrix) * x.rgb;
	return x;
}
