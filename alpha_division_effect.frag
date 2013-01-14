// Note: Division by zero will give inf or nan, whose conversion to
// integer types is implementation-defined. However, anything is fine for
// alpha=0, since that's undefined anyway.
vec4 FUNCNAME(vec2 tc) {
	vec4 x = INPUT(tc);
	x.rgb /= x.aaa;	
	return x;
}
