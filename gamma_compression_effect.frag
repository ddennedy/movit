// Compress gamma curve.

// Implicit uniforms:
// uniform float PREFIX(linear_scale);
// uniform float PREFIX(c)[5];
// uniform float PREFIX(clog)[5];
// uniform float PREFIX(beta);
// uniform float PREFIX(lambda);

vec4 FUNCNAME(vec2 tc) {
	vec4 x = INPUT(tc);

	// We could reasonably get values outside (0.0, 1.0), but the formulas below
	// are not valid outside that range, so clamp before we do anything else.
	x.rgb = clamp(x.rgb, 0.0, 1.0);

	vec3 a = x.rgb * PREFIX(linear_scale);

	// Fourth-order polynomial approximation to pow(). See the .cpp file for details.
	vec3 s = sqrt(x.rgb);
	vec3 b = PREFIX(c)[0] + (PREFIX(c)[1] + (PREFIX(c)[2] + (PREFIX(c)[3] + PREFIX(c)[4] * s) * s) * s) * s;
	vec3 c = PREFIX(clog)[0] + (PREFIX(clog)[1] + (PREFIX(clog)[2] + (PREFIX(clog)[3] + PREFIX(clog)[4] * s) * s) * s) * s;

	vec3 f = vec3(greaterThan(x.rgb, vec3(PREFIX(beta))));
	vec3 g = vec3(greaterThan(x.rgb, vec3(PREFIX(lambda))));
	x = vec4(mix(mix(a, b, f), c, g), x.a);

	return x;
}
