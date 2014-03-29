// DIRECTION_VERTICAL will be #defined to 1 if we are scaling vertically,
// and 0 otherwise.

uniform sampler2D PREFIX(sample_tex);
uniform int PREFIX(num_samples);
uniform float PREFIX(num_loops);
uniform float PREFIX(sample_x_scale);
uniform float PREFIX(sample_x_offset);
uniform float PREFIX(slice_height);

// We put the fractional part of the offset (-0.5 to 0.5 pixels) in the weights
// because we have to (otherwise they'd do nothing). However, the support texture
// has limited numerical precision and we'd need as much of it as we can for
// getting the subpixel sampling right, and adding a large constant to each value
// will reduce the precision further. Thus, the non-fractional part of the offset
// is sent in through a uniform that we simply add in the beginning of the shader.
uniform float PREFIX(whole_pixel_offset);

// Sample a single weight. First fetch information about where to sample
// and the weight from sample_tex, and then read the pixel itself.
vec4 PREFIX(do_sample)(vec2 tc, int i)
{
	vec2 sample_tc;
	sample_tc.x = float(i) * PREFIX(sample_x_scale) + PREFIX(sample_x_offset);
#if DIRECTION_VERTICAL
	sample_tc.y = tc.y * PREFIX(num_loops);
#else
	sample_tc.y = tc.x * PREFIX(num_loops);
#endif
	vec2 sample = tex2D(PREFIX(sample_tex), sample_tc).rg;

#if DIRECTION_VERTICAL
	tc.y = sample.g + floor(sample_tc.y) * PREFIX(slice_height);
#else
	tc.x = sample.g + floor(sample_tc.y) * PREFIX(slice_height);
#endif
	return vec4(sample.r) * INPUT(tc);
}

vec4 FUNCNAME(vec2 tc) {
#if DIRECTION_VERTICAL
	tc.y += PREFIX(whole_pixel_offset);
#else
	tc.x += PREFIX(whole_pixel_offset);
#endif
	vec4 sum = PREFIX(do_sample)(tc, 0);
	for (int i = 1; i < PREFIX(num_samples); ++i) {
		sum += PREFIX(do_sample)(tc, i);
	}
	return sum;
}

#undef DIRECTION_VERTICAL
