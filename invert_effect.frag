// Used only during testing. Inverts its input.
vec4 FUNCNAME(vec2 tc)
{
	vec4 rgba = INPUT(tc);
	rgba.rgb = vec3(1.0) - rgba.rgb;
	return rgba;
}
