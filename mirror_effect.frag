// Mirrors the image horizontally.
vec4 FUNCNAME(vec2 tc)
{
	tc = vec2(1.0, 0.0) + tc * vec2(-1.0, 1.0);
	return LAST_INPUT(tc);
}
