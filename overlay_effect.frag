// If we didn't have to worry about alpha in the bottom layer,
// this would be a simple mix(). However, since people might
// compose multiple layers together and we don't really have
// any control over the order, it's better to do it right.
//
// These formulas come from Wikipedia:
//
//   http://en.wikipedia.org/wiki/Alpha_compositing

vec4 FUNCNAME(vec2 tc) {
	vec4 bottom = INPUT1(tc);
	vec4 top = INPUT2(tc);
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
}
