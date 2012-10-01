// Compress to Rec. 601/Rec. 709 gamma curve.

#if 0

// if we have the lut
uniform sampler1D PREFIX(rec709_inverse_tex);

vec4 FUNCNAME(vec2 tc) {
	vec4 x = LAST_INPUT(tc);

	x.r = texture1D(PREFIX(rec709_inverse_tex), x.r).x;
	x.g = texture1D(PREFIX(rec709_inverse_tex), x.g).x;
	x.b = texture1D(PREFIX(rec709_inverse_tex), x.b).x;

	return x;
}

#else

// use arithmetic (slow)
vec4 FUNCNAME(vec2 tc) {
	vec4 x = LAST_INPUT(tc);

	vec3 a = vec3(4.5) * x.rgb;
	vec3 b = vec3(1.099) * pow(x.rgb, vec3(0.45)) - vec3(0.099);
	vec3 f = vec3(greaterThan(x.rgb, vec3(0.018)));

	return vec4(mix(a, b, f), x.a); 
}
 
#endif
