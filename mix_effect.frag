vec4 FUNCNAME(vec2 tc) {
	vec4 first = INPUT1(tc);
	vec4 second = INPUT2(tc);
	vec4 result = vec4(PREFIX(strength_first)) * first + vec4(PREFIX(strength_second)) * second;

	// Clamping alpha at some stage, either here or in AlphaDivisionEffect,
	// is actually very important for some use cases. Consider, for instance,
	// the case where we have additive blending (strength_first = strength_second = 1),
	// and add two 50% gray 100% opaque (0.5, 0.5, 0.5, 1.0) pixels. Without
	// alpha clamping, we'd get (1.0, 1.0, 1.0, 2.0), which would then in
	// conversion to postmultiplied be divided back to (0.5, 0.5, 0.5)!
	// Clamping alpha to 1.0 fixes the problem, and we get the expected result
	// of (1.0, 1.0, 1.0). Similarly, adding (0.5, 0.5, 0.5, 0.5) to itself
	// yields (1.0, 1.0, 1.0, 1.0) (100% white 100% opaque), which makes sense.
	//
	// The classic way of doing additive blending with premultiplied alpha
	// is to give the additive component alpha=0, but this also doesn't make
	// sense in a world where we could end up postmultiplied; just consider
	// the case where we have first=(0, 0, 0, 0) (ie., completely transparent)
	// and second=(0.5, 0.5, 0.5, 0.5) (ie., white at 50% opacity).
	// Zeroing out the alpha of second would yield (0.5, 0.5, 0.5, 0.0),
	// which has undefined RGB values in postmultiplied storage; certainly
	// e.g. (0, 0, 0, 0) would not be an expected output. Also, it would
	// break the expectation that A+B = B+A.
	//
	// Note that we do _not_ clamp RGB, since it might be useful to have
	// out-of-gamut colors. We could choose to do the alpha clamping in
	// AlphaDivisionEffect instead, though; I haven't thought a lot about
	// if that would be better or not.
	result.a = clamp(result.a, 0.0, 1.0);

	return result;
}
