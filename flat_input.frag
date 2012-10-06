uniform sampler2D PREFIX(tex);

vec4 FUNCNAME(vec2 tc) {
	// OpenGL's origin is bottom-left, but most graphics software assumes
	// a top-left origin. Thus, for inputs that come from the user,
	// we flip the y coordinate.
	tc.y = 1.0 - tc.y;

	return texture2D(PREFIX(tex), tc);
}
