// Used only for testing.

// Implicit uniforms:
// uniform vec2 PREFIX(offset);

vec4 FUNCNAME(vec2 tc)
{
	return INPUT(tc * 2.0 + PREFIX(offset));
//	vec2 z = tc * 2.0 + PREFIX(offset);
//	return vec4(z.y, 0.0f, 0.0f, 1.0f);
}
