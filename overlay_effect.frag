// It's actually (but surprisingly) not correct to do a mix() here;
// it would be if we had postmultiplied alpha and didn't have to worry
// about alpha in the bottom layer, but given that we use premultiplied
// alpha all over, top shouldn't actually be multiplied by anything.
//
// These formulas come from Wikipedia:
//
//   http://en.wikipedia.org/wiki/Alpha_compositing
//
// We use the associative version given. However, note that since we want
// _output_ to be premultiplied, C_o from Wikipedia is not what we want,
// but rather c_o (which is not explicitly given, but obviously is just
// C_o without the division by alpha_o).

vec4 FUNCNAME(vec2 tc) {
	vec4 bottom = INPUT1(tc);
	vec4 top = INPUT2(tc);
#if 0
	// Postmultiplied version.
	float new_alpha = mix(bottom.a, 1.0, top.a);
	if (new_alpha < 1e-6) {
		// new_alpha = 0 only if top.a = bottom.a = 0, at least as long as
		// both alphas are within range. (If they're not, the result is not
		// meaningful anyway.) And if new_alpha = 0, we don't really have
		// any meaningful output anyway, so just set it to zero instead of
		// getting division-by-zero below.
		return vec4(0.0);
	} else {
		vec3 premultiplied_color = mix(bottom.rgb * bottom.aaa, top.rgb, top.a);
		vec3 color = premultiplied_color / new_alpha;
		return vec4(color.r, color.g, color.b, new_alpha);
	}
#else
	return top + (1.0 - top.a) * bottom;
#endif
}
