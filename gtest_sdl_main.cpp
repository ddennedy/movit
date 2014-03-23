#define GTEST_HAS_EXCEPTIONS 0

#ifdef HAVE_SDL2
#include <SDL2/SDL.h>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_video.h>
#else
#include <SDL/SDL.h>
#include <SDL/SDL_error.h>
#include <SDL/SDL_video.h>
#endif
#include <stdio.h>
#include <stdlib.h>

#include "gtest/gtest.h"

int main(int argc, char **argv) {
	// Set up an OpenGL context using SDL.
	if (SDL_Init(SDL_INIT_VIDEO) == -1) {
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		exit(1);
	}
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

#ifdef HAVE_SDL2
	// You can uncomment this if you want to try a core context.
	// For Mesa, you can get the same effect by doing
	//
	//   export MESA_GL_VERSION_OVERRIDE=3.1FC
	//
	// before running tests.
//	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
//	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
//	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

	// See also init.cpp for how to enable debugging.
//	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);

	SDL_Window *window = SDL_CreateWindow("OpenGL window for unit test",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		32, 32,
		SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(window);
	assert(context != NULL);
#else
	SDL_SetVideoMode(32, 32, 0, SDL_OPENGL);
	SDL_WM_SetCaption("OpenGL window for unit test", NULL);
#endif

	testing::InitGoogleTest(&argc, argv);
	int err = RUN_ALL_TESTS();
	SDL_Quit();
	exit(err);
}
