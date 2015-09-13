// Implicit uniforms:
// uniform sampler2D PREFIX(tex);

vec4 FUNCNAME(vec2 tc) {
	// OpenGL's origin is bottom-left, but most graphics software assumes
	// a top-left origin. Thus, for inputs that come from the user,
	// we flip the y coordinate.
	tc.y = 1.0 - tc.y;

	vec4 pixel = tex2D(PREFIX(tex), tc);

	// These two are #defined to 0 or 1 in flat_input.cpp.
#if FIXUP_SWAP_RB
	pixel.rb = pixel.br;
#endif
#if FIXUP_RED_TO_GRAYSCALE
	pixel.gb = pixel.rr;
#endif
	return pixel;
}

#undef FIXUP_SWAP_RB
#undef FIXUP_RED_TO_GRAYSCALE
