// Compress to sRGB gamma curve.

#if 0

// if we have the lut
uniform sampler1D PREFIX(srgb_inverse_tex);

vec4 FUNCNAME(vec2 tc) {
	vec4 x = LAST_INPUT(tc);

	x.r = texture1D(PREFIX(srgb_inverse_tex), x.r).x;
	x.g = texture1D(PREFIX(srgb_inverse_tex), x.g).x;
	x.b = texture1D(PREFIX(srgb_inverse_tex), x.b).x;

	return x;
}

#else

// use arithmetic (slow)
vec4 FUNCNAME(vec2 tc) {
	vec4 x = LAST_INPUT(tc);

	vec3 a = vec3(12.92) * x.rgb;
	vec3 b = vec3(1.055) * pow(x.rgb, vec3(1.0/2.4)) - vec3(0.055);
	vec3 f = vec3(greaterThan(x.rgb, vec3(0.0031308)));

	return vec4(mix(a, b, f), x.a); 
}
 
#endif
