// Compress to sRGB gamma curve.

vec4 FUNCNAME(vec2 tc) {
	vec4 x = INPUT(tc);

	x.r = texture1D(PREFIX(compression_curve_tex), x.r).x;
	x.g = texture1D(PREFIX(compression_curve_tex), x.g).x;
	x.b = texture1D(PREFIX(compression_curve_tex), x.b).x;

	return x;
}
