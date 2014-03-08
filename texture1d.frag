uniform sampler1D tex;
varying vec2 tc;

void main()
{
	gl_FragColor = texture1D(tex, tc.x);
}
