#define GTEST_HAS_EXCEPTIONS 0

#include <SDL2/SDL.h>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_video.h>
#ifdef HAVE_BENCHMARK
#include <benchmark/benchmark.h>
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

	// Use a core context, because Mesa only allows certain OpenGL versions in core.
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

	// See also init.cpp for how to enable debugging.
//	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);

	SDL_Window *window = SDL_CreateWindow("OpenGL window for unit test",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		32, 32,
		SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(window);
	assert(context != nullptr);

	int err;
	if (argc >= 2 && strcmp(argv[1], "--benchmark") == 0) {
#ifdef HAVE_BENCHMARK
		--argc;
		::benchmark::Initialize(&argc, argv + 1);
		if (::benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
		::benchmark::RunSpecifiedBenchmarks();
		err = 0;
#else
		fprintf(stderr, "No support for microbenchmarks compiled in.\n");
		err = 1;
#endif
	} else {
		testing::InitGoogleTest(&argc, argv);
		err = RUN_ALL_TESTS();
	}
	SDL_Quit();
	return err;
}
