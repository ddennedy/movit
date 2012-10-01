uniform sampler2D input_tex;
varying vec2 tc;

vec4 read_input(vec2 tc)
{
	vec3 x = texture2D(input_tex, tc.st);
}

#define LAST_INPUT read_input
