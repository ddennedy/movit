// A simple, circular vignette, with a cos² falloff.
	
uniform float PREFIX(inv_radius);
varying vec2 PREFIX(normalized_pos);

vec4 FUNCNAME(vec2 tc) {
	vec4 x = LAST_INPUT(tc);

	const float pihalf = 0.5 * 3.14159265358979324;

	float dist = (length(PREFIX(normalized_pos)) - PREFIX(inner_radius)) * PREFIX(inv_radius);
	float linear_falloff = clamp(dist, 0.0, 1.0) * pihalf;
	float falloff = cos(linear_falloff) * cos(linear_falloff);
	x.rgb *= vec3(falloff);

	return x;
}
