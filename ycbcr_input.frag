uniform sampler2D PREFIX(tex_y);
uniform sampler2D PREFIX(tex_cb);
uniform sampler2D PREFIX(tex_cr);

vec4 FUNCNAME(vec2 tc) {
	// OpenGL's origin is bottom-left, but most graphics software assumes
	// a top-left origin. Thus, for inputs that come from the user,
	// we flip the y coordinate.
	tc.y = 1.0 - tc.y;

	vec3 ycbcr;
	ycbcr.x = texture2D(PREFIX(tex_y), tc).x;
	ycbcr.y = texture2D(PREFIX(tex_cb), tc + PREFIX(cb_offset)).x;
	ycbcr.z = texture2D(PREFIX(tex_cr), tc + PREFIX(cr_offset)).x;

	ycbcr -= PREFIX(offset);
	ycbcr *= PREFIX(scale);

	vec4 rgba;
	rgba.rgb = PREFIX(inv_ycbcr_matrix) * ycbcr;
	rgba.a = 1.0;
	return rgba;
}
