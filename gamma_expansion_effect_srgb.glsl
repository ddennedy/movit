// Expand sRGB gamma curve.

#if 0

// if we have the lut
uniform sampler1D PREFIX(srgb_tex);

vec4 FUNCNAME(vec2 tc) {
	vec4 x = LAST_INPUT(tc);

	x.r = texture1D(PREFIX(srgb_tex), x.r).x;
	x.g = texture1D(PREFIX(srgb_tex), x.g).x;
	x.b = texture1D(PREFIX(srgb_tex), x.b).x;

	return x;
}

#else

// use arithmetic (slow)
vec4 FUNCNAME(vec2 tc) {
	vec4 x = LAST_INPUT(tc);

	vec3 a = x.rgb * vec3(1.0/12.92); 
	vec3 b = pow((x.rgb + vec3(0.055)) * vec3(1.0/1.055), vec3(2.4));
	vec3 f = vec3(greaterThan(x.rgb, vec3(0.04045)));

	return vec4(mix(a, b, f), x.a); 
}
 
#endif
