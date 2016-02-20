// Expand gamma curve.

// Implicit uniforms:
// uniform float PREFIX(linear_scale);
// uniform float PREFIX(c)[5];
// uniform float PREFIX(beta);

vec4 FUNCNAME(vec2 tc) {
	vec4 x = INPUT(tc);

	vec3 a = x.rgb * PREFIX(linear_scale);

	// Fourth-order polynomial approximation to pow(). See the .cpp file for details.
	vec3 b = PREFIX(c[0]) + (PREFIX(c[1]) + (PREFIX(c[2]) + (PREFIX(c[3]) + PREFIX(c[4]) * x.rgb) * x.rgb) * x.rgb) * x.rgb;

	vec3 f = vec3(greaterThan(x.rgb, vec3(PREFIX(beta))));
	x = vec4(mix(a, b, f), x.a);

	return x;
}
