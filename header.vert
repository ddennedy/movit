vec2 read_input(vec2 tc)
{
	return gl_MultiTexCoord0.st;
}

#define LAST_INPUT read_input
