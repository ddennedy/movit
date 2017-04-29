// Implicit uniforms:
// uniform sampler2D PREFIX(tex_y);
// uniform sampler2D PREFIX(tex_cbcr);  // If CB_CR_SAME_TEXTURE.
// uniform sampler2D PREFIX(tex_cb);    // If not CB_CR_SAME_TEXTURE.
// uniform sampler2D PREFIX(tex_cr);    // If not CB_CR_SAME_TEXTURE.
// uniform mat3 PREFIX(ycbcr_matrix);
// uniform vec3 PREFIX(offset);
// uniform vec2 PREFIX(cb_offset);
// uniform vec2 PREFIX(cr_offset);

vec4 FUNCNAME(vec2 tc) {
	// OpenGL's origin is bottom-left, but most graphics software assumes
	// a top-left origin. Thus, for inputs that come from the user,
	// we flip the y coordinate.
	tc.y = 1.0 - tc.y;

	vec3 ycbcr;
#if Y_CB_CR_SAME_TEXTURE
	ycbcr = tex2D(PREFIX(tex_y), tc).xyz;
#else
	ycbcr.x = tex2D(PREFIX(tex_y), tc).x;
  #if CB_CR_SAME_TEXTURE
    #if CB_CR_OFFSETS_EQUAL
	ycbcr.yz = tex2D(PREFIX(tex_cbcr), tc + PREFIX(cb_offset)).xy;
    #else
	ycbcr.y = tex2D(PREFIX(tex_cbcr), tc + PREFIX(cb_offset)).x;
	ycbcr.z = tex2D(PREFIX(tex_cbcr), tc + PREFIX(cr_offset)).x;
    #endif
  #else
	ycbcr.y = tex2D(PREFIX(tex_cb), tc + PREFIX(cb_offset)).x;
	ycbcr.z = tex2D(PREFIX(tex_cr), tc + PREFIX(cr_offset)).x;
  #endif
#endif

	ycbcr -= PREFIX(offset);

	vec4 rgba;
	rgba.rgb = PREFIX(inv_ycbcr_matrix) * ycbcr;
	rgba.a = 1.0;
	return rgba;
}
