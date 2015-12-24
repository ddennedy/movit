// Implicit uniforms:
// uniform float PREFIX(output_coord_to_slice_num);
// uniform float PREFIX(slice_num_to_input_coord);
// uniform float PREFIX(slice_offset_to_input_coord);
// uniform float PREFIX(normalized_offset);
 
vec4 FUNCNAME(vec2 tc) {
	// DIRECTION_VERTICAL will be #defined to 1 if we are expanding vertically,
	// and 0 otherwise.
#if DIRECTION_VERTICAL
	float sliced_coord = 1.0 - tc.y;
#else
	float sliced_coord = tc.x;
#endif

	// Find out which slice we are in, and a 0..1 coordinate for the offset within that slice.
	float slice_num = floor(sliced_coord * PREFIX(output_coord_to_slice_num));
	float slice_offset = fract(sliced_coord * PREFIX(output_coord_to_slice_num));

	// Find out where this slice begins in the input data, and then offset from that.
	float input_coord = slice_num * PREFIX(slice_num_to_input_coord) + slice_offset * PREFIX(slice_offset_to_input_coord) + PREFIX(normalized_offset);

#if DIRECTION_VERTICAL
	return INPUT(vec2(tc.x, 1.0 - input_coord));
#else
	return INPUT(vec2(input_coord, tc.y));
#endif
}

#undef DIRECTION_VERTICAL
