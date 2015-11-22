// Implicit uniforms:
// uniform int PREFIX(current_field_position);
// uniform float PREFIX(num_lines);
// uniform float PREFIX(self_offset);
// uniform float PREFIX(inv_width);
// uniform float PREFIX(current_offset)[2];
// uniform float PREFIX(other_offset)[3];

// The best explanation of YADIF that I've seen is actually a pseudocode
// reimplementation from the Doom9 forum:
//
//   http://forum.doom9.org/showthread.php?p=980375#post980375
//
// We generally follow its terminology instead of the original C source
// (which I'll refer to as “C YADIF”), although I've used the C source as a
// reference to double-check at times. We're not bit-exact the same as
// C YADIF; in particular, we work in linear light, and left/right edge
// handling might also be a bit different (for top/bottom edge handling,
// C YADIF repeats texels like we do). Also, C YADIF generally works on
// Y', Cb and Cr planes separately, while we work on the entire RGBA triplet
// and do our spatial interpolation decisions based on the pixel as a whole,
// so our decision metric also naturally becomes different.

#define DIFF(s1, s2) dot((s1) - (s2), (s1) - (s2))

vec4 FUNCNAME(vec2 tc) {
	int yi = int(round(tc.y * PREFIX(num_lines) - 0.5f));

	// Figure out if we just want to keep the current line or if
	// we need to interpolate. This branch is obviously divergent,
	// but the very nature of deinterlacing would seem to require that.
	//
	// Note that since we have bottom-left origin, yi % 2 will return 0
	// for bottom and 1 for top.
	if ((yi % 2) != PREFIX(current_field_position)) {
		return INPUT3(vec2(tc.x, tc.y + PREFIX(self_offset)));
	}

	// First, estimate the current pixel from the neighboring pixels in the
	// same field (spatial interpolation). We try first 0 degrees (straight
	// up/down), then ±45 degrees and then finally ±63 degrees. The best of
	// these, as determined by the “spatial score” (basically sum of squared
	// differences in three neighboring pixels), is kept.
	//
	// The C version of YADIF goesn't check +63° unless +45° gave an improvement,
	// and similarly not -63° unless -45° did. The MMX version goes through pains
	// to simulate the same, but notes that it “hurts both quality and speed”.
	// We're not bit-exact the same as the C version anyway, and not sampling
	// ±63° would probably be a rather divergent branch, so we just always do it.

	// a b c d e f g     ↑ y
	//       x           |
	// h i j k l m n     +--> x

	vec2 a_pos = vec2(tc.x - 3.0 * PREFIX(inv_width), tc.y + PREFIX(current_offset)[1]);
	vec2 b_pos = vec2(tc.x - 2.0 * PREFIX(inv_width), a_pos.y);
	vec2 c_pos = vec2(tc.x -       PREFIX(inv_width), a_pos.y);
	vec2 d_pos = vec2(tc.x,                           a_pos.y);
	vec2 e_pos = vec2(tc.x +       PREFIX(inv_width), a_pos.y);
	vec2 f_pos = vec2(tc.x + 2.0 * PREFIX(inv_width), a_pos.y);
	vec2 g_pos = vec2(tc.x + 3.0 * PREFIX(inv_width), a_pos.y);

	vec2 h_pos = vec2(tc.x - 3.0 * PREFIX(inv_width), tc.y + PREFIX(current_offset)[0]);
	vec2 i_pos = vec2(tc.x - 2.0 * PREFIX(inv_width), h_pos.y);
	vec2 j_pos = vec2(tc.x -       PREFIX(inv_width), h_pos.y);
	vec2 k_pos = vec2(tc.x,                           h_pos.y);
	vec2 l_pos = vec2(tc.x +       PREFIX(inv_width), h_pos.y);
	vec2 m_pos = vec2(tc.x + 2.0 * PREFIX(inv_width), h_pos.y);
	vec2 n_pos = vec2(tc.x + 3.0 * PREFIX(inv_width), h_pos.y);

	vec4 a = INPUT3(a_pos);
	vec4 b = INPUT3(b_pos);
	vec4 c = INPUT3(c_pos);
	vec4 d = INPUT3(d_pos);
	vec4 e = INPUT3(e_pos);
	vec4 f = INPUT3(f_pos);
	vec4 g = INPUT3(g_pos);
	vec4 h = INPUT3(h_pos);
	vec4 i = INPUT3(i_pos);
	vec4 j = INPUT3(j_pos);
	vec4 k = INPUT3(k_pos);
	vec4 l = INPUT3(l_pos);
	vec4 m = INPUT3(m_pos);
	vec4 n = INPUT3(n_pos);

	// 0 degrees. Note that pred is actually twice the real spatial prediction;
	// we halve it later to same some arithmetic. Also, our spatial score is not
	// the same as in C YADIF; we use the total squared sum over all four
	// channels instead of deinterlacing each channel separately.
	//
	// Note that there's a small, arbitrary bonus for this first alternative,
	// so that vertical interpolation wins if everything else is equal.
	vec4 pred = d + k;
	float score;
	float best_score = DIFF(c, j) + DIFF(d, k) + DIFF(e, l) - 1e-4;

	// -45 degrees.
	score = DIFF(b, k) + DIFF(c, l) + DIFF(d, m);
	if (score < best_score) {
		pred = c + l;
		best_score = score;
	}

	// -63 degrees.
	score = DIFF(a, l) + DIFF(b, m) + DIFF(c, n);
	if (score < best_score) {
		pred = b + m;
		best_score = score;
	}

	// +45 degrees.
	score = DIFF(d, i) + DIFF(e, j) + DIFF(f, k);
	if (score < best_score) {
		pred = e + j;
		best_score = score;
	}

	// +63 degrees.
	score = DIFF(e, h) + DIFF(f, i) + DIFF(g, j);
	if (score < best_score) {
		pred = f + i;
		// best_score isn't used anymore.
	}

	pred *= 0.5f;

	// Now we do a temporal prediction (p2) of this pixel based on the previous
	// and next fields. The spatial prediction is clamped so that it is not
	// too far from this temporal prediction, where “too far” is based on
	// the amount of local temporal change. (In other words, the temporal prediction
	// is the safe choice, and the question is how far away from that we'll let
	// our spatial choice run.) Note that here, our difference metric
	// _is_ the same as C YADIF, namely per-channel abs.
	//
	// The sample positions look like this; in order to avoid variable name conflicts
	// with the spatial interpolation, we use uppercase names. x is, again,
	// the current pixel we're trying to estimate.
	//
	//     C   H      ↑ y
	//   A   F   K    |
	//     D x I      |
	//   B   G   L    |
	//     E   J      +-----> time
	//
	vec2 AFK_pos = d_pos;
	vec2 BGL_pos = k_pos;
	vec4 A = INPUT1(AFK_pos);
	vec4 B = INPUT1(BGL_pos);
	vec4 F = d;
	vec4 G = k;
	vec4 K = INPUT5(AFK_pos);
	vec4 L = INPUT5(BGL_pos);

	vec2 CH_pos = vec2(tc.x, tc.y + PREFIX(other_offset)[2]);
	vec2 DI_pos = vec2(tc.x, tc.y + PREFIX(other_offset)[1]);
	vec2 EJ_pos = vec2(tc.x, tc.y + PREFIX(other_offset)[0]);

	vec4 C = INPUT2(CH_pos);
	vec4 D = INPUT2(DI_pos);
	vec4 E = INPUT2(EJ_pos);

	vec4 H = INPUT4(CH_pos);
	vec4 I = INPUT4(DI_pos);
	vec4 J = INPUT4(EJ_pos);

	// Find temporal differences around this line, using all five fields.
	// tdiff0 is around the current field, tdiff1 is around the previous one,
	// tdiff2 is around the next one.
	vec4 tdiff0 = abs(D - I);
	vec4 tdiff1 = abs(A - F) + abs(B - G);  // Actually twice tdiff1.
	vec4 tdiff2 = abs(K - F) + abs(L - G);  // Actually twice tdiff2.
	vec4 diff = max(tdiff0, 0.5f * max(tdiff1, tdiff2));

	// The following part is the spatial interlacing check, which loosens up the
	// allowable temporal change. (See also the comments in the .h file.)
	// It costs us four extra loads (C, E, H, J) and a few extra ALU ops;
	// we're already very load-heavy, so the extra ALU is effectively free.
	// It costs about 18% performance in some benchmarks, which squares
	// well with going from 20 to 24 loads (a 20% increase), although for
	// total overall performance in longer chains, the difference is nearly zero.
	//
	// The basic idea is seemingly to allow more change if there are large spatial
	// vertical changes, even if there are few temporal changes. These differences
	// are signed, though, which make it more tricky to follow, although they seem
	// to reduce into some sort of pseudo-abs. I will not claim to understand them
	// very well.
	//
	// We start by temporally interpolating the current vertical line (p0–p4):
	//
	//     C p0 H      ↑ y
	//   A   p1   K    |
	//     D p2 I      |
	//   B   p3   L    |
	//     E p4 J      +-----> time
	//
	// YADIF_ENABLE_SPATIAL_INTERLACING_CHECK will be #defined to 1
	// if the check is enabled. Otherwise, the compiler should
	// be able to remove the dependent code quite easily.
	vec4 p0 = 0.5f * (C + H);
	vec4 p1 = F;
	vec4 p2 = 0.5f * (D + I);
	vec4 p3 = G;
	vec4 p4 = 0.5f * (E + J);

#if YADIF_ENABLE_SPATIAL_INTERLACING_CHECK
	vec4 max_ = max(max(p2 - p3, p2 - p1), min(p0 - p1, p4 - p3));
	vec4 min_ = min(min(p2 - p3, p2 - p1), max(p0 - p1, p4 - p3));
	diff = max(diff, max(min_, -max_));
#endif

	return clamp(pred, p2 - diff, p2 + diff);
}

#undef DIFF
#undef YADIF_ENABLE_SPATIAL_INTERLACING_CHECK
