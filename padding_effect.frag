// Implicit uniforms:
// uniform vec2 PREFIX(offset);
// uniform vec2 PREFIX(scale);
//
// uniform vec2 PREFIX(normalized_coords_to_texels);
// uniform vec2 PREFIX(offset_bottomleft);
// uniform vec2 PREFIX(offset_topright);

vec4 FUNCNAME(vec2 tc) {
	tc -= PREFIX(offset);
	tc *= PREFIX(scale);

	vec2 tc_texels = tc * PREFIX(normalized_coords_to_texels);
	vec2 coverage_bottomleft = clamp(tc_texels + PREFIX(offset_bottomleft), 0.0f, 1.0f);
	vec2 coverage_topright = clamp(PREFIX(offset_topright) - tc_texels, 0.0f, 1.0f);
	vec2 coverage_both = coverage_bottomleft * coverage_topright;
	float coverage = coverage_both.x * coverage_both.y;

	if (coverage <= 0.0f) {
		// Short-circuit in case the underlying function is expensive to call.
		return PREFIX(border_color);
	} else {
		return mix(PREFIX(border_color), INPUT(tc), coverage);
	}
}
