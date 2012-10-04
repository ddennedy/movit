// Saturate/desaturate (in linear space).

vec4 FUNCNAME(vec2 tc) {
	vec4 x = INPUT(tc);

	float luminance = dot(x.rgb, vec3(0.2126, 0.7152, 0.0722));
	x.rgb = mix(vec3(luminance), x.rgb, PREFIX(saturation));

	return x;
}
