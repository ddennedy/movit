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
	return top + (1.0 - top.a) * bottom;
}
