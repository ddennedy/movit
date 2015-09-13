// A simple, circular vignette, with a cos² falloff.

// Implicit uniforms:
// uniform float PREFIX(pihalf_div_radius);
//
// uniform vec2 PREFIX(aspect_correction);
// uniform vec2 PREFIX(flipped_center);

vec4 FUNCNAME(vec2 tc) {
	vec4 x = INPUT(tc);

	const float pihalf = 0.5 * 3.14159265358979324;

	vec2 normalized_pos = (tc - PREFIX(flipped_center)) * PREFIX(aspect_correction);
	float dist = (length(normalized_pos) - PREFIX(inner_radius)) * PREFIX(pihalf_div_radius);
	float linear_falloff = clamp(dist, 0.0, pihalf);
	float falloff = cos(linear_falloff) * cos(linear_falloff);
	x.rgb *= vec3(falloff);

	return x;
}
