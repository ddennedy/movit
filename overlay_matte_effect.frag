vec4 FUNCNAME(vec2 tc) {
	vec4 orig = INPUT1(tc);
	vec4 blurred = INPUT2(tc);
	float luminance = clamp(dot(orig.rgb, vec3(0.2126, 0.7152, 0.0722)), 0.0, 1.0);
	return mix(orig, blurred, luminance * vec4(PREFIX(blurred_mix_amount)));
}
