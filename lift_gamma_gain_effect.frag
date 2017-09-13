// Implicit uniforms:
//
// These are calculated in the host code to save some arithmetic.
// uniform vec3 PREFIX(gain_pow_inv_gamma);  // gain^(1/gamma).
// uniform vec3 PREFIX(inv_gamma_22);  // 2.2 / gamma.

vec4 FUNCNAME(vec2 tc) {
	vec4 x = INPUT(tc);

	x.rgb /= x.aaa;

	// pow() of negative numbers is undefined, so clip out-of-gamut values.
	x.rgb = max(x.rgb, 0.0);

	x.rgb = pow(x.rgb, vec3(1.0/2.2));
	x.rgb += PREFIX(lift) * (vec3(1) - x.rgb);

	// Clip out-of-gamut values again.
	x.rgb = max(x.rgb, 0.0);

	x.rgb = pow(x.rgb, PREFIX(inv_gamma_22));
	x.rgb *= PREFIX(gain_pow_inv_gamma);
	x.rgb *= x.aaa;

	return x;
}
