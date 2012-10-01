#version 120
uniform sampler2D tex;
varying vec4 tc;
uniform vec3 lift, gain;
uniform vec3 gain_pow_inv_gamma, inv_gamma_22;
uniform float saturation;

#if 0
// if we have the lut
uniform sampler1D srgb_tex, srgb_reverse_tex;
vec3 to_linear(vec3 x) {
	vec3 ret;
	ret.r = texture1D(srgb_tex, x.r).x;
	ret.g = texture1D(srgb_tex, x.g).x;
	ret.b = texture1D(srgb_tex, x.b).x;
	return ret;
}
vec3 from_linear(vec3 x) {
	vec3 ret;
	ret.r = texture1D(srgb_reverse_tex, x.r).x;
	ret.g = texture1D(srgb_reverse_tex, x.g).x;
	ret.b = texture1D(srgb_reverse_tex, x.b).x;
	return ret;
}
#else
// use arithmetic (slow)
vec3 to_linear(vec3 x) {
	vec3 a = x * vec3(1.0/12.92); 
	vec3 b = pow((x + vec3(0.055)) * vec3(1.0/1.055), vec3(2.4));
	vec3 f = vec3(greaterThan(x, vec3(0.04045)));
	return mix(a, b, f); 
} 
vec3 from_linear(vec3 x) {
	vec3 a = vec3(12.92) * x;
	vec3 b = vec3(1.055) * pow(x, vec3(1.0/2.4)) - vec3(0.055);
	vec3 f = vec3(greaterThan(x, vec3(0.0031308)));
	return mix(a, b, f);
}
#endif

void main()
{
	vec3 x = texture2D(tex, tc.st, -30.0f).rgb;
	x = to_linear(x);

	// do lift in nonlinear space (the others don't care)
	x = pow(x, vec3(1.0/2.2));
	x += lift * (vec3(1) - x);
	x = pow(x, inv_gamma_22);
	x *= gain_pow_inv_gamma;

	// LMS correction
//	x = colorMat * x;

	// saturate/desaturate (in linear space)
	float luminance = dot(x, vec3(0.2126, 0.7152, 0.0722));
	x = mix(vec3(luminance), x, saturation);

	gl_FragColor.rgb = from_linear(x);
	gl_FragColor.a = 1.0f;
}
