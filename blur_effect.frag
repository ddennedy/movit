// A simple, very stupid horizontal blur. Will be fixed soonish.

vec4 FUNCNAME(vec2 tc) {
	vec4 x = LAST_INPUT(tc);
	return
		vec4(0.1) * LAST_INPUT(tc + vec2(-0.010f, 0.0)) +
		vec4(0.1) * LAST_INPUT(tc + vec2(-0.008f, 0.0)) +
		vec4(0.1) * LAST_INPUT(tc + vec2(-0.006f, 0.0)) +
		vec4(0.1) * LAST_INPUT(tc + vec2(-0.004f, 0.0)) +
		vec4(0.2) * LAST_INPUT(tc + vec2(-0.002f, 0.0)) +
		vec4(0.3) * LAST_INPUT(tc + vec2(-0.000f, 0.0)) +
		vec4(0.2) * LAST_INPUT(tc + vec2( 0.002f, 0.0)) +
		vec4(0.1) * LAST_INPUT(tc + vec2( 0.004f, 0.0)) +
		vec4(0.1) * LAST_INPUT(tc + vec2( 0.006f, 0.0)) +
		vec4(0.1) * LAST_INPUT(tc + vec2( 0.008f, 0.0)) +
		vec4(0.1) * LAST_INPUT(tc + vec2( 0.010f, 0.0));
}
