varying vec2 tc;

void main()
{
	tc = LAST_INPUT(tc);
        gl_Position = ftransform();
}
