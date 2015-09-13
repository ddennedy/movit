// Implicit uniforms:
// uniform sampler2D PREFIX(tex_y);
// uniform sampler2D PREFIX(tex_cbcr);

vec4 FUNCNAME(vec2 tc) {
	// OpenGL's origin is bottom-left, but most graphics software assumes
	// a top-left origin. Thus, for inputs that come from the user,
	// we flip the y coordinate.
	tc.y = 1.0 - tc.y;

	vec3 ycbcr;
	ycbcr.x = tex2D(PREFIX(tex_y), tc).y;
#if CB_CR_OFFSETS_EQUAL
	vec2 tc_cbcr = tc;
	tc_cbcr.x += PREFIX(cb_offset_x);
	ycbcr.yz = tex2D(PREFIX(tex_cbcr), tc_cbcr).xz;
#else
	vec2 tc_cb = tc;
	tc_cb.x += PREFIX(cb_offset_x);
	ycbcr.y = tex2D(PREFIX(tex_cbcr), tc_cb).x;

	vec2 tc_cr = tc;
	tc_cr.x += PREFIX(cr_offset_x);
	ycbcr.z = tex2D(PREFIX(tex_cbcr), tc_cr).z;
#endif

	ycbcr -= PREFIX(offset);

	vec4 rgba;
	rgba.rgb = PREFIX(inv_ycbcr_matrix) * ycbcr;
	rgba.a = 1.0;
	return rgba;
}
