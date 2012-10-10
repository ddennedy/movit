uniform vec4 PREFIX(samples)[(R + 1) * (R + 1)];

vec4 FUNCNAME(vec2 tc) {
	// The full matrix has five different symmetry cases, that look like this:
	//
	// D * * C * * D
	// * D * C * D *
	// * * D C D * *
	// B B B A B B B
	// * * D C D * *
	// * D * C * D *
	// D * * C * * D
	//
	// We only store the lower-right part of the matrix:
	//
	// A B B 
	// C D *
	// C * D

	// Case A: Top-left sample has no symmetry.
	vec4 sum = PREFIX(samples)[0].z * INPUT(tc);

	// Case B: Uppermost samples have left/right symmetry.
	for (int x = 1; x <= R; ++x) {
		vec4 sample = PREFIX(samples)[x];
		sum += sample.z * (INPUT(tc - sample.xy) + INPUT(tc + sample.xy));
	}

	// Case C: Leftmost samples have top/bottom symmetry.
	for (int y = 1; y <= R; ++y) {
		vec4 sample = PREFIX(samples)[y * (R + 1)];
		sum += sample.z * (INPUT(tc - sample.xy) + INPUT(tc + sample.xy));
	}

	// Case D: Diagonal samples have four-way symmetry.
	for (int xy = 1; xy <= R; ++xy) {
		vec4 sample = PREFIX(samples)[xy * (R + 1) + xy];
		vec2 mirror_sample = vec2(sample.x, -sample.y);

		vec4 local_sum = INPUT(tc - sample.xy) + INPUT(tc + sample.xy);
		local_sum += INPUT(tc - mirror_sample.xy) + INPUT(tc + mirror_sample.xy);

		sum += sample.z * local_sum;
	}

	// Case *: All other samples have eight-way symmetry.
	for (int y = 1; y < R; ++y) {
		for (int x = y + 1; x <= R; ++x) {
			vec4 sample = PREFIX(samples)[y * (R + 1) + x];
			vec2 mirror_sample = vec2(sample.x, -sample.y);

			vec4 local_sum = INPUT(tc - sample.xy) + INPUT(tc + sample.xy);
			local_sum += INPUT(tc - mirror_sample.xy) + INPUT(tc + mirror_sample.xy);
			local_sum += INPUT(tc - sample.yx) + INPUT(tc + sample.yx);
			local_sum += INPUT(tc - mirror_sample.yx) + INPUT(tc + mirror_sample.yx);

			sum += sample.z * local_sum;
		}
	}

	return sum;
}
