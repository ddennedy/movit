uniform vec2 PREFIX(offset);
uniform vec2 PREFIX(scale);
uniform vec2 PREFIX(texcoord_min);
uniform vec2 PREFIX(texcoord_max);

vec4 FUNCNAME(vec2 tc) {
	tc -= PREFIX(offset);
	tc *= PREFIX(scale);

#if 1
	if (any(lessThan(tc, PREFIX(texcoord_min))) ||
	    any(greaterThan(tc, PREFIX(texcoord_max)))) {
		return PREFIX(border_color);
	}
#endif
	if (any(lessThan(tc, vec2(0.0))) ||
	    any(greaterThan(tc, vec2(1.0)))) {
		return PREFIX(border_color);
	} 
#if 0
	// In theory, maybe we should test on the outmost textel centers
	// (e.g. (0.5/width, 0.5/height) for the top-left) instead of the
	// texture border. However, since we only support integer padding
	if (any(lessThan(tc, vec2(0.0))) || any(greaterThan(tc, vec2(1.0)))) {
		return PREFIX(border_color);
	}
#endif

	return INPUT(tc);
}
