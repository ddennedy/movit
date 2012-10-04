uniform sampler2D input_tex;
varying vec2 tc;

vec4 read_input(vec2 tc)
{
	return texture2D(input_tex, tc.st);
}

#define INPUT read_input
