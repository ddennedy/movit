varying vec2 tc;

void main()
{
	tc = LAST_INPUT();
        gl_Position = ftransform();
}
