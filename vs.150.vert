#version 150

in vec2 position;
in vec2 texcoord;
out vec2 tc;

// Will be overridden by compile_glsl_program() if needed.
// (It cannot just be prepended, as #version must be before everything.)
#define FLIP_ORIGIN 0

void main()
{
	// The result of glOrtho(0.0, 1.0, 0.0, 1.0, 0.0, 1.0) is:
	//
	//   2.000  0.000  0.000 -1.000
	//   0.000  2.000  0.000 -1.000
	//   0.000  0.000 -2.000 -1.000
	//   0.000  0.000  0.000  1.000
	gl_Position = vec4(2.0 * position.x - 1.0, 2.0 * position.y - 1.0, -1.0, 1.0);
	tc = texcoord;
#if FLIP_ORIGIN
	tc.y = 1.0f - tc.y;
#endif
}
