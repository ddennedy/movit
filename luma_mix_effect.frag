// Implicit uniforms:
// uniform float PREFIX(progress_mul_w_plus_one);
// uniform bool PREFIX(bool_inverse);

vec4 FUNCNAME(vec2 tc) {
	vec4 first = INPUT1(tc);
	vec4 second = INPUT2(tc);

	// We treat the luma as going from 0 to w, where w is the transition width
	// (wider means the boundary between transitioned and non-transitioned
	// will be harder, while w=0 is essentially just a straight fade).
	// We need to map this 0..w range in the luma image to a (clamped) 0..1
	// range for how far this pixel has come in a fade. At the very
	// beginning, we can visualize it like this, where every pixel is in
	// the state 0.0 (100% first image, 0% second image):
	//
	//         0                     w
	//   luma: |---------------------|
	//   mix:                        |----|
	//                               0    1
	//
	// Then as we progress, eventually the luma range should move to the right
	// so that more pixels start moving towards higher mix value:
	//
	//           0                     w
	//   luma:   |---------------------|
	//   mix:                        |----|
	//                               0    1
	//
	// and at the very end, all pixels should be in the state 1.0 (0% first image,
	// 100% second image):
	//
	//                                    0                     w
	//   luma:                            |---------------------|
	//   mix:                        |----|
	//                               0    1
	//
	// So clearly, it should move (w+1) units to the right, and apart from that
	// just stay a simple mapping.
	float w = PREFIX(transition_width);
	float luma = INPUT3(tc).x;
	if (PREFIX(bool_inverse)) {
		luma = 1.0 - luma;
	}
	float m = clamp((luma * w - w) + PREFIX(progress_mul_w_plus_one), 0.0, 1.0);

	return mix(first, second, m);
}
