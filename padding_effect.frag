uniform vec2 PREFIX(offset);
uniform vec2 PREFIX(scale);
uniform vec2 PREFIX(texcoord_min);
uniform vec2 PREFIX(texcoord_max);

vec4 FUNCNAME(vec2 tc) {
	tc -= PREFIX(offset);
	tc *= PREFIX(scale);

	if (any(lessThan(tc, PREFIX(texcoord_min))) ||
	    any(greaterThan(tc, PREFIX(texcoord_max)))) {
		return PREFIX(border_color);
	}

	return INPUT(tc);
}
