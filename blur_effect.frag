// A simple un.directional blur.
// DIRECTION_VERTICAL will be #defined to 1 if we are doing a vertical blur,
// 0 otherwise.

// Implicit uniforms:
// uniform vec2 PREFIX(samples)[NUM_TAPS / 2 + 1];

vec4 FUNCNAME(vec2 tc) {
	vec4 sum = vec4(PREFIX(samples)[0].y) * INPUT(tc);
	for (int i = 1; i < NUM_TAPS / 2 + 1; ++i) {
		vec2 sample = PREFIX(samples)[i];
		vec2 sample1_tc = tc, sample2_tc = tc;
#if DIRECTION_VERTICAL
		sample1_tc.y -= sample.x;
		sample2_tc.y += sample.x;
#else
		sample1_tc.x -= sample.x;
		sample2_tc.x += sample.x;
#endif
		sum += vec4(sample.y) * (INPUT(sample1_tc) + INPUT(sample2_tc));
	}
	return sum;
}

#undef DIRECTION_VERTICAL
