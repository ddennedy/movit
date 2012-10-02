// A simple, very stupid horizontal blur. Will be fixed soonish.

uniform float PREFIX(pixel_offset);
uniform float PREFIX(weight)[15];

vec4 FUNCNAME(vec2 tc) {
	vec4 x = LAST_INPUT(tc);
	return
		vec4(PREFIX(weight)[ 0]) * LAST_INPUT(vec2(tc.x - 7.0 * PREFIX(pixel_offset), tc.y)) +
		vec4(PREFIX(weight)[ 1]) * LAST_INPUT(vec2(tc.x - 6.0 * PREFIX(pixel_offset), tc.y)) +
		vec4(PREFIX(weight)[ 2]) * LAST_INPUT(vec2(tc.x - 5.0 * PREFIX(pixel_offset), tc.y)) +
		vec4(PREFIX(weight)[ 3]) * LAST_INPUT(vec2(tc.x - 4.0 * PREFIX(pixel_offset), tc.y)) +
		vec4(PREFIX(weight)[ 4]) * LAST_INPUT(vec2(tc.x - 3.0 * PREFIX(pixel_offset), tc.y)) +
		vec4(PREFIX(weight)[ 5]) * LAST_INPUT(vec2(tc.x - 2.0 * PREFIX(pixel_offset), tc.y)) +
		vec4(PREFIX(weight)[ 6]) * LAST_INPUT(vec2(tc.x -       PREFIX(pixel_offset), tc.y)) +
		vec4(PREFIX(weight)[ 7]) * LAST_INPUT(tc) +
		vec4(PREFIX(weight)[ 8]) * LAST_INPUT(vec2(tc.x +       PREFIX(pixel_offset), tc.y)) +
		vec4(PREFIX(weight)[ 9]) * LAST_INPUT(vec2(tc.x + 2.0 * PREFIX(pixel_offset), tc.y)) +
		vec4(PREFIX(weight)[10]) * LAST_INPUT(vec2(tc.x + 3.0 * PREFIX(pixel_offset), tc.y)) +
		vec4(PREFIX(weight)[11]) * LAST_INPUT(vec2(tc.x + 4.0 * PREFIX(pixel_offset), tc.y)) +
		vec4(PREFIX(weight)[12]) * LAST_INPUT(vec2(tc.x + 5.0 * PREFIX(pixel_offset), tc.y));
		vec4(PREFIX(weight)[13]) * LAST_INPUT(vec2(tc.x + 6.0 * PREFIX(pixel_offset), tc.y));
		vec4(PREFIX(weight)[14]) * LAST_INPUT(vec2(tc.x + 7.0 * PREFIX(pixel_offset), tc.y));
}
