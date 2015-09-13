// Expand gamma curve.

// Implicit uniforms:
// uniform float PREFIX(linear_scale);
// uniform float PREFIX(c0), PREFIX(c1), PREFIX(c2), PREFIX(c3), PREFIX(c4);
// uniform float PREFIX(beta);

vec4 FUNCNAME(vec2 tc) {
	vec4 x = INPUT(tc);

	vec3 a = x.rgb * PREFIX(linear_scale);

	// Fourth-order polynomial approximation to pow(). See the .cpp file for details.
	vec3 b = PREFIX(c0) + (PREFIX(c1) + (PREFIX(c2) + (PREFIX(c3) + PREFIX(c4) * x.rgb) * x.rgb) * x.rgb) * x.rgb;

	vec3 f = vec3(greaterThan(x.rgb, vec3(PREFIX(beta))));
	x = vec4(mix(a, b, f), x.a);

	return x;
}
