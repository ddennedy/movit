varying vec2 tc;

void main()
{
	tc = INPUT();
        gl_Position = ftransform();
}
