#define NO_SDL_GLEXT 1

#define WIDTH 1280
#define HEIGHT 720

#include <epoxy/gl.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_mouse.h>
#include <SDL2/SDL_video.h>

#include <assert.h>
#include <features.h>
#include <math.h>
#include <png.h>
#include <pngconf.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include "diffusion_effect.h"
#include "effect.h"
#include "effect_chain.h"
#include "flat_input.h"
#include "image_format.h"
#include "init.h"
#include "lift_gamma_gain_effect.h"
#include "saturation_effect.h"
#include "util.h"
#include "widgets.h"

using namespace movit;

unsigned char result[WIDTH * HEIGHT * 4];

float lift_theta = 0.0f, lift_rad = 0.0f, lift_v = 0.0f;
float gamma_theta = 0.0f, gamma_rad = 0.0f, gamma_v = 0.5f;
float gain_theta = 0.0f, gain_rad = 0.0f, gain_v = 0.25f;
float saturation = 1.0f;

//float radius = 0.3f;
// float inner_radius = 0.3f;
float blur_radius = 20.0f;
float blurred_mix_amount = 0.5f;
	
void update_hsv(Effect *lift_gamma_gain_effect, Effect *saturation_effect)
{
	RGBTriplet lift(0.0f, 0.0f, 0.0f);
	RGBTriplet gamma(1.0f, 1.0f, 1.0f);
	RGBTriplet gain(1.0f, 1.0f, 1.0f);

	hsv2rgb_normalized(lift_theta, lift_rad, lift_v, &lift.r, &lift.g, &lift.b);
	hsv2rgb_normalized(gamma_theta, gamma_rad, gamma_v * 2.0f, &gamma.r, &gamma.g, &gamma.b);
	hsv2rgb_normalized(gain_theta, gain_rad, gain_v * 4.0f, &gain.r, &gain.g, &gain.b);

	bool ok = lift_gamma_gain_effect->set_vec3("lift", (float *)&lift);
	ok = ok && lift_gamma_gain_effect->set_vec3("gamma", (float *)&gamma);
	ok = ok && lift_gamma_gain_effect->set_vec3("gain", (float *)&gain);
	assert(ok);

	if (saturation < 0.0) {
		saturation = 0.0;
	}
	ok = saturation_effect->set_float("saturation", saturation);
	assert(ok);
}

void mouse(int x, int y)
{
	float xf = (x / (float)WIDTH) * 16.0f / 9.0f;
	float yf = (HEIGHT - y) / (float)HEIGHT;

	if (yf < 0.2f) {
		read_colorwheel(xf, yf, &lift_rad, &lift_theta, &lift_v);
	} else if (yf >= 0.2f && yf < 0.4f) {
		read_colorwheel(xf, yf - 0.2f, &gamma_rad, &gamma_theta, &gamma_v);
	} else if (yf >= 0.4f && yf < 0.6f) {
		read_colorwheel(xf, yf - 0.4f, &gain_rad, &gain_theta, &gain_v);
	} else if (yf >= 0.6f && yf < 0.62f && xf < 0.2f) {
		saturation = (xf / 0.2f) * 4.0f;
#if 0
	} else if (yf >= 0.65f && yf < 0.67f && xf < 0.2f) {
		radius = (xf / 0.2f);
	} else if (yf >= 0.70f && yf < 0.72f && xf < 0.2f) {
		inner_radius = (xf / 0.2f);
#endif
	} else if (yf >= 0.75f && yf < 0.77f && xf < 0.2f) {
		blur_radius = (xf / 0.2f) * 100.0f;
	} else if (yf >= 0.80f && yf < 0.82f && xf < 0.2f) {
		blurred_mix_amount = (xf / 0.2f);
	}
}

unsigned char *load_image(const char *filename, unsigned *w, unsigned *h)
{
	SDL_Surface *img = IMG_Load(filename);
	if (img == nullptr) {
		fprintf(stderr, "Load of '%s' failed\n", filename);
		exit(1);
	}

	SDL_PixelFormat rgba_fmt;
	rgba_fmt.palette = nullptr;
	rgba_fmt.BitsPerPixel = 32;
	rgba_fmt.BytesPerPixel = 8;
	rgba_fmt.Rloss = rgba_fmt.Gloss = rgba_fmt.Bloss = rgba_fmt.Aloss = 0;

	// NOTE: Assumes little endian.
	rgba_fmt.Rmask = 0x00ff0000;
	rgba_fmt.Gmask = 0x0000ff00;
	rgba_fmt.Bmask = 0x000000ff;
	rgba_fmt.Amask = 0xff000000;

	rgba_fmt.Rshift = 16;
	rgba_fmt.Gshift = 8;
	rgba_fmt.Bshift = 0;
	rgba_fmt.Ashift = 24;

	SDL_Surface *converted = SDL_ConvertSurface(img, &rgba_fmt, SDL_SWSURFACE);

	*w = img->w;
	*h = img->h;

	SDL_FreeSurface(img);

	return (unsigned char *)converted->pixels;
}

void write_png(const char *filename, unsigned char *screenbuf)
{
	FILE *fp = fopen(filename, "wb");
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	png_infop info_ptr = png_create_info_struct(png_ptr);
	
	if (setjmp(png_jmpbuf(png_ptr))) {
		fclose(fp);
		fprintf(stderr, "Write to %s failed; exiting.\n", filename);
		exit(1);
	}

	png_set_IHDR(png_ptr, info_ptr, WIDTH, HEIGHT, 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	png_bytep *row_pointers = new png_bytep[HEIGHT];
	for (unsigned y = 0; y < HEIGHT; ++y) {
		row_pointers[y] = screenbuf + ((HEIGHT - y - 1) * WIDTH) * 4;
	}

	png_init_io(png_ptr, fp);
	png_set_rows(png_ptr, info_ptr, row_pointers);
	png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_BGR, nullptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(fp);

	delete[] row_pointers;
}

int main(int argc, char **argv)
{
	bool quit = false;

	if (SDL_Init(SDL_INIT_EVERYTHING) == -1) {
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		exit(1);
	}
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	// SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
	SDL_Window *window = SDL_CreateWindow("OpenGL window",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		WIDTH, HEIGHT,
		SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(window);
	assert(context != nullptr);

	CHECK(init_movit(".", MOVIT_DEBUG_ON));
	printf("GPU texture subpixel precision: about %.1f bits\n",
		log2(1.0f / movit_texel_subpixel_precision));
	printf("Wrongly rounded x+0.48 or x+0.52 values: %d/510\n",
		movit_num_wrongly_rounded);
	if (movit_num_wrongly_rounded > 0) {
		printf("Rounding off in the shader to compensate.\n");
	}
	
	unsigned img_w, img_h;
	unsigned char *src_img = load_image(argc > 1 ? argv[1] : "blg_wheels_woman_1.jpg", &img_w, &img_h);

	EffectChain chain(WIDTH, HEIGHT);
	glViewport(0, 0, WIDTH, HEIGHT);

	ImageFormat inout_format;
	inout_format.color_space = COLORSPACE_sRGB;
	inout_format.gamma_curve = GAMMA_sRGB;

	FlatInput *input = new FlatInput(inout_format, FORMAT_BGRA_POSTMULTIPLIED_ALPHA, GL_UNSIGNED_BYTE, img_w, img_h);
	chain.add_input(input);
	Effect *lift_gamma_gain_effect = chain.add_effect(new LiftGammaGainEffect());
	Effect *saturation_effect = chain.add_effect(new SaturationEffect());
	Effect *diffusion_effect = chain.add_effect(new DiffusionEffect());
	//Effect *vignette_effect = chain.add_effect(new VignetteEffect());
	//Effect *sandbox_effect = chain.add_effect(new SandboxEffect());
	//sandbox_effect->set_float("parm", 42.0f);
	//chain.add_effect(new MirrorEffect());
	chain.add_output(inout_format, OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
	chain.set_dither_bits(8);
	chain.finalize();

	// generate a PBO to hold the data we read back with glReadPixels()
	// (Intel/DRI goes into a slow path if we don't read to PBO)
	GLuint pbo;
	glGenBuffers(1, &pbo);
	glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, pbo);
	glBufferData(GL_PIXEL_PACK_BUFFER_ARB, WIDTH * HEIGHT * 4, nullptr, GL_STREAM_READ);

	init_hsv_resources();
	check_error();

	int frame = 0;
	bool screenshot = false;
#if _POSIX_C_SOURCE >= 199309L
	struct timespec start, now;
	clock_gettime(CLOCK_MONOTONIC, &start);
#else
	struct timeval start, now;
	gettimeofday(&start, nullptr);
#endif

	while (!quit) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				quit = true;
			} else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
				quit = true;
			} else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F1) {
				screenshot = true;
			} else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
				mouse(event.button.x, event.button.y);
			} else if (event.type == SDL_MOUSEMOTION && (event.motion.state & SDL_BUTTON(1))) {
				mouse(event.motion.x, event.motion.y);
			}
		}

		++frame;

		update_hsv(lift_gamma_gain_effect, saturation_effect);
		//vignette_effect->set_float("radius", radius);
		//vignette_effect->set_float("inner_radius", inner_radius);
		//vignette_effect->set_vec2("center", (float[]){ 0.7f, 0.5f });

		CHECK(diffusion_effect->set_float("radius", blur_radius));
		CHECK(diffusion_effect->set_float("blurred_mix_amount", blurred_mix_amount));

		input->set_pixel_data(src_img);
		chain.render_to_screen();
		
		glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, pbo);
		check_error();
		glReadPixels(0, 0, WIDTH, HEIGHT, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, BUFFER_OFFSET(0));
		check_error();
		glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);
		check_error();

		draw_hsv_wheel(0.0f, lift_rad, lift_theta, lift_v);
		draw_hsv_wheel(0.2f, gamma_rad, gamma_theta, gamma_v);
		draw_hsv_wheel(0.4f, gain_rad, gain_theta, gain_v);
		draw_saturation_bar(0.6f, saturation / 4.0f);
#if 0
		draw_saturation_bar(0.65f, radius);
		draw_saturation_bar(0.70f, inner_radius);
#endif
		draw_saturation_bar(0.75f, blur_radius / 100.0f);
		draw_saturation_bar(0.80f, blurred_mix_amount);

		SDL_GL_SwapWindow(window);
		check_error();

		glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, pbo);
		check_error();
		unsigned char *screenbuf = (unsigned char *)glMapBuffer(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY);
		check_error();
		if (screenshot) {
			char filename[256];
			sprintf(filename, "frame%05d.png", frame);
			write_png(filename, screenbuf);
			printf("Screenshot: %s\n", filename);
			screenshot = false;
		}
		glUnmapBuffer(GL_PIXEL_PACK_BUFFER_ARB);
		check_error();
		glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);
		check_error();

#if 1
#if _POSIX_C_SOURCE >= 199309L
		clock_gettime(CLOCK_MONOTONIC, &now);
		double elapsed = now.tv_sec - start.tv_sec +
			1e-9 * (now.tv_nsec - start.tv_nsec);
#else
		gettimeofday(&now, nullptr);
		double elapsed = now.tv_sec - start.tv_sec +
			1e-6 * (now.tv_usec - start.tv_usec);
#endif
		printf("%d frames in %.3f seconds = %.1f fps (%.1f ms/frame)\n",
			frame, elapsed, frame / elapsed,
			1e3 * elapsed / frame);

		// Reset every 100 frames, so that local variations in frame times
		// (especially for the first few frames, when the shaders are
		// compiled etc.) don't make it hard to measure for the entire
		// remaining duration of the program.
		if (frame == 100) {
			frame = 0;
			start = now;
		}
#endif
	}
	cleanup_hsv_resources();
	return 0; 
}
