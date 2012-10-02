// Expand Rec. 601/Rec. 709 gamma curve.

#if 0

// if we have the lut
uniform sampler1D PREFIX(rec709_tex);

vec4 FUNCNAME(vec2 tc) {
	vec4 x = LAST_INPUT(tc);

	x.r = texture1D(PREFIX(rec709_tex), x.r).x;
	x.g = texture1D(PREFIX(rec709_tex), x.g).x;
	x.b = texture1D(PREFIX(rec709_tex), x.b).x;

	return x;
}

#else

// use arithmetic (slow)
vec4 FUNCNAME(vec2 tc) {
	vec4 x = LAST_INPUT(tc);

	vec3 a = x.rgb * vec3(1.0/4.500);
	vec3 b = pow((x.rgb + vec3(0.099)) * vec3(1.0/1.099), vec3(1.0/0.45));
	vec3 f = vec3(greaterThan(x.rgb, vec3(0.081)));

	return vec4(mix(a, b, f), x.a); 
}
 
#endif
