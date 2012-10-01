// Standard lift/gamma/gain color correction tools.
//
// We do lift in a nonlinear (gamma-2.2) space since that looks a lot better
// than in linear (blacks stay a lot closer to black). The two others don't
// really care; they are (sans some constants) commutative with the x^2.2
// operation.

// These are calculated in the host code to save some arithmetic.
uniform vec3 PREFIX(gain_pow_inv_gamma), PREFIX(inv_gamma_22);

vec4 FUNCNAME(vec2 tc) {
	vec4 x = LAST_INPUT(tc);

	// do lift in nonlinear space (the others don't care)
	x.rgb = pow(x.rgb, vec3(1.0/2.2));
	x.rgb += PREFIX(lift) * (vec3(1) - x.rgb);
	x.rgb = pow(x.rgb, PREFIX(inv_gamma_22));
	x.rgb *= PREFIX(gain_pow_inv_gamma);

	return x;
}
