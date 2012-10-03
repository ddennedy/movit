// A simple unidirectional blur.

#define NUM_TAPS 16

uniform vec4 PREFIX(samples)[NUM_TAPS + 1];

vec4 FUNCNAME(vec2 tc) {
	vec4 sum = vec4(PREFIX(samples)[0].z) * LAST_INPUT(tc);
	for (int i = 1; i < NUM_TAPS + 1; ++i) {
		vec4 sample = PREFIX(samples)[i];
		sum += vec4(sample.z) * (LAST_INPUT(tc - sample.xy) + LAST_INPUT(tc + sample.xy));
	}
	return sum;
}
