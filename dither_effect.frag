uniform sampler2D PREFIX(dither_tex);
uniform vec2 PREFIX(tc_scale);

vec4 FUNCNAME(vec2 tc) {
	// We also choose to dither alpha, just in case.
	// Maybe it should in theory have a separate dither,
	// but I doubt it matters much. We currently don't
	// really handle alpha in any case.
	return INPUT(tc) + texture2D(PREFIX(dither_tex), tc * PREFIX(tc_scale)).xxxx;
}
