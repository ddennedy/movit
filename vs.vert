varying vec2 tc;

void main()
{
	tc = gl_MultiTexCoord0.st;
        gl_Position = ftransform();
}
