#version 130

in vec2 tc;

vec4 tex2D(sampler2D s, vec2 coord)
{
	return texture(s, coord);
}
