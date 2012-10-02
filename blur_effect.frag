// A simple unidirectional blur.

uniform vec2 PREFIX(pixel_offset);
uniform float PREFIX(weight)[15];

vec4 FUNCNAME(vec2 tc) {
	vec4 x = LAST_INPUT(tc);
	return
		vec4(PREFIX(weight)[ 0]) * LAST_INPUT(tc - 7.0 * PREFIX(pixel_offset)) +
		vec4(PREFIX(weight)[ 1]) * LAST_INPUT(tc - 6.0 * PREFIX(pixel_offset)) +
		vec4(PREFIX(weight)[ 2]) * LAST_INPUT(tc - 5.0 * PREFIX(pixel_offset)) +
		vec4(PREFIX(weight)[ 3]) * LAST_INPUT(tc - 4.0 * PREFIX(pixel_offset)) +
		vec4(PREFIX(weight)[ 4]) * LAST_INPUT(tc - 3.0 * PREFIX(pixel_offset)) +
		vec4(PREFIX(weight)[ 5]) * LAST_INPUT(tc - 2.0 * PREFIX(pixel_offset)) +
		vec4(PREFIX(weight)[ 6]) * LAST_INPUT(tc -       PREFIX(pixel_offset)) +
		vec4(PREFIX(weight)[ 7]) * LAST_INPUT(tc) +
		vec4(PREFIX(weight)[ 8]) * LAST_INPUT(tc +       PREFIX(pixel_offset)) +
		vec4(PREFIX(weight)[ 9]) * LAST_INPUT(tc + 2.0 * PREFIX(pixel_offset)) +
		vec4(PREFIX(weight)[10]) * LAST_INPUT(tc + 3.0 * PREFIX(pixel_offset)) +
		vec4(PREFIX(weight)[11]) * LAST_INPUT(tc + 4.0 * PREFIX(pixel_offset)) +
		vec4(PREFIX(weight)[12]) * LAST_INPUT(tc + 5.0 * PREFIX(pixel_offset)) +
		vec4(PREFIX(weight)[13]) * LAST_INPUT(tc + 6.0 * PREFIX(pixel_offset)) +
		vec4(PREFIX(weight)[14]) * LAST_INPUT(tc + 7.0 * PREFIX(pixel_offset));
}
