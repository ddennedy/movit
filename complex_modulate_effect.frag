// Implicit uniforms:
// uniform vec2 PREFIX(num_repeats);

vec4 FUNCNAME(vec2 tc) {
	vec4 pixel = INPUT1(tc);
	vec2 pattern = INPUT2(tc * PREFIX(num_repeats)).xy;

	// Complex multiplication between each of (pixel.xy, pixel.zw) and pattern.xy.
	return pattern.x * pixel + pattern.y * vec4(-pixel.y, pixel.x, -pixel.w, pixel.z);
}
