#define GL_GLEXT_PROTOTYPES 1
#define NO_SDL_GLEXT 1

#define WIDTH 1280
#define HEIGHT 720
#define BUFFER_OFFSET(i) ((char *)NULL + (i))

#define HSV_WHEEL_SIZE 128

#include <math.h>
#include <time.h>

#include <SDL/SDL.h>
#include <SDL/SDL_opengl.h>
#include <SDL/SDL_image.h>

#include <GL/gl.h>
#include <GL/glext.h>

#ifdef NDEBUG
#define check_error()
#else
#define check_error() { int err = glGetError(); if (err != GL_NO_ERROR) { printf("GL error 0x%x at line %d\n", err, __LINE__); exit(1); } }
#endif

unsigned char result[WIDTH * HEIGHT * 4];

float lift_theta = 0.0f, lift_rad = 0.0f, lift_v = 0.0f;
float gamma_theta = 0.0f, gamma_rad = 0.0f, gamma_v = 0.5f;
float gain_theta = 0.0f, gain_rad = 0.0f, gain_v = 0.25f;
float saturation = 1.0f;

float lift_r = 0.0f, lift_g = 0.0f, lift_b = 0.0f;
float gamma_r = 1.0f, gamma_g = 1.0f, gamma_b = 1.0f;
float gain_r = 1.0f, gain_g = 1.0f, gain_b = 1.0f;

enum textures {
	SOURCE_IMAGE = 1,
	SRGB_LUT = 2,
	SRGB_REVERSE_LUT = 3,
	HSV_WHEEL = 4,
};

// assumes h in [0, 2pi> or [-pi, pi>
void hsv2rgb(float h, float s, float v, float *r, float *g, float *b)
{
	if (h < 0.0f) {
		h += 2.0f * M_PI;
	}
	float c = v * s;
	float hp = (h * 180.0 / M_PI) / 60.0;
	float x = c * (1 - fabs(fmod(hp, 2.0f) - 1.0f));

	if (hp >= 0 && hp < 1) {
		*r = c;
		*g = x;
		*b = 0.0f;
	} else if (hp >= 1 && hp < 2) {
		*r = x;
		*g = c;
		*b = 0.0f;
	} else if (hp >= 2 && hp < 3) {
		*r = 0.0f;
		*g = c;
		*b = x;
	} else if (hp >= 3 && hp < 4) {
		*r = 0.0f;
		*g = x;
		*b = c;
	} else if (hp >= 4 && hp < 5) {
		*r = x;
		*g = 0.0f;
		*b = c;
	} else {
		*r = c;
		*g = 0.0f;
		*b = x;
	}

	float m = v - c;
	*r += m;
	*g += m;
	*b += m;
}

GLhandleARB read_shader(const char* filename, GLenum type)
{
	static char buf[131072];
	FILE *fp = fopen(filename, "r");
	if (fp == NULL) {
		perror(filename);
		exit(1);
	}

	int len = fread(buf, 1, sizeof(buf), fp);
	fclose(fp);

	GLhandleARB obj = glCreateShaderObjectARB(type);
	const GLchar* source[] = { buf };
	const GLint length[] = { len };
	glShaderSource(obj, 1, source, length);
	glCompileShader(obj);

	GLchar info_log[4096];
	GLsizei log_length = sizeof(info_log) - 1;
	glGetShaderInfoLog(obj, log_length, &log_length, info_log);
	info_log[log_length] = 0; 
	printf("shader compile log: %s\n", info_log);

	GLint status;
	glGetShaderiv(obj, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		exit(1);
	}

	return obj;
}

void draw_picture_quad(GLint prog, int frame)
{
	glUseProgramObjectARB(prog);
	check_error();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, SOURCE_IMAGE);
	glUniform1i(glGetUniformLocation(prog, "tex"), 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_1D, SRGB_LUT);
	glUniform1i(glGetUniformLocation(prog, "srgb_tex"), 1);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_1D, SRGB_REVERSE_LUT);
	glUniform1i(glGetUniformLocation(prog, "srgb_reverse_tex"), 2);

	glUniform3f(glGetUniformLocation(prog, "lift"), lift_r, lift_g, lift_b);
	//glUniform3f(glGetUniformLocation(prog, "gamma"), gamma_r, gamma_g, gamma_b);
	glUniform3f(glGetUniformLocation(prog, "inv_gamma_22"),
	            2.2f / gamma_r,
	            2.2f / gamma_g,
	            2.2f / gamma_b);
	glUniform3f(glGetUniformLocation(prog, "gain_pow_inv_gamma"),
	            pow(gain_r, 1.0f / gamma_r),
	            pow(gain_g, 1.0f / gamma_g),
	            pow(gain_b, 1.0f / gamma_b));
	glUniform1f(glGetUniformLocation(prog, "saturation"), saturation);

	glDisable(GL_BLEND);
	check_error();
	glDisable(GL_DEPTH_TEST);
	check_error();
	glDepthMask(GL_FALSE);
	check_error();

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, 1.0, 0.0, 1.0, 0.0, 1.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
//	glClear(GL_COLOR_BUFFER_BIT);
	check_error();

	glBegin(GL_QUADS);

	glTexCoord2f(0.0f, 1.0f);
	glVertex2f(0.0f, 0.0f);

	glTexCoord2f(1.0f, 1.0f);
	glVertex2f(1.0f, 0.0f);

	glTexCoord2f(1.0f, 0.0f);
	glVertex2f(1.0f, 1.0f);

	glTexCoord2f(0.0f, 0.0f);
	glVertex2f(0.0f, 1.0f);

	glEnd();
	check_error();
}

void draw_hsv_wheel(float y, float rad, float theta, float value)
{
	glUseProgramObjectARB(0);
	check_error();
	glActiveTexture(GL_TEXTURE0);
	check_error();
	glEnable(GL_TEXTURE_2D);
	check_error();
	glBindTexture(GL_TEXTURE_2D, HSV_WHEEL);
	check_error();
	glActiveTexture(GL_TEXTURE1);
	check_error();
	glBindTexture(GL_TEXTURE_2D, 0);
	check_error();
	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_BLEND);
	check_error();
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	check_error();

	// wheel
	glBegin(GL_QUADS);

	glTexCoord2f(0.0f, 1.0f);
	glVertex2f(0.0f, y);

	glTexCoord2f(1.0f, 1.0f);
	glVertex2f(0.2f * 9.0f / 16.0f, y);

	glTexCoord2f(1.0f, 0.0f);
	glVertex2f(0.2f * 9.0f / 16.0f, y + 0.2f);

	glTexCoord2f(0.0f, 0.0f);
	glVertex2f(0.0f, y + 0.2f);

	glEnd();

	// wheel selector
	glDisable(GL_TEXTURE_2D);
	glColor3f(0.0f, 0.0f, 0.0f);
	glPointSize(5.0f);
	glBegin(GL_POINTS);
	glVertex2f((0.1f + rad * cos(theta) * 0.1f) * 9.0f / 16.0f, y + 0.1f - rad * sin(theta) * 0.1f);
	glEnd();
	
	// value slider
	glDisable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);

	glColor3f(0.0f, 0.0f, 0.0f);
	glVertex2f(0.22f * 9.0f / 16.0f, y);
	glVertex2f(0.24f * 9.0f / 16.0f, y);

	glColor3f(1.0f, 1.0f, 1.0f);
	glVertex2f(0.24f * 9.0f / 16.0f, y + 0.2f);
	glVertex2f(0.22f * 9.0f / 16.0f, y + 0.2f);

	glEnd();

	// value selector
	glColor3f(0.0f, 0.0f, 0.0f);
	glPointSize(5.0f);
	glBegin(GL_POINTS);
	glVertex2f(0.23f * 9.0f / 16.0f, y + value * 0.2f);
	glEnd();

	glColor3f(1.0f, 1.0f, 1.0f);
}

void draw_saturation_bar(float y)
{
	glUseProgramObjectARB(0);
	check_error();

	// value slider
	glDisable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);

	glColor3f(0.0f, 0.0f, 0.0f);
	glVertex2f(0.0f * 9.0f / 16.0f, y + 0.02f);
	glVertex2f(0.0f * 9.0f / 16.0f, y);

	glColor3f(1.0f, 1.0f, 1.0f);
	glVertex2f(0.2f * 9.0f / 16.0f, y);
	glVertex2f(0.2f * 9.0f / 16.0f, y + 0.02f);

	glEnd();

	// value selector
	glColor3f(0.0f, 0.0f, 0.0f);
	glPointSize(5.0f);
	glBegin(GL_POINTS);
	glVertex2f(0.2f * (saturation / 4.0f) * 9.0f / 16.0f, y + 0.01f);
	glEnd();

	glColor3f(1.0f, 1.0f, 1.0f);
}

void make_hsv_wheel_texture()
{
	static unsigned char hsv_pix[HSV_WHEEL_SIZE * HSV_WHEEL_SIZE * 4];
	for (int y = 0; y < HSV_WHEEL_SIZE; ++y) {
		for (int x = 0; x < HSV_WHEEL_SIZE; ++x) {
			float yf = 2.0f * y / (float)(HSV_WHEEL_SIZE) - 1.0f;
			float xf = 2.0f * x / (float)(HSV_WHEEL_SIZE) - 1.0f;
			float rad = hypot(xf, yf);
			float theta = atan2(yf, xf);

			float r, g, b;
			hsv2rgb(theta, rad, 1.0f, &r, &g, &b);
			hsv_pix[(y * HSV_WHEEL_SIZE + x) * 4 + 0] = lrintf(r * 255.0f);
			hsv_pix[(y * HSV_WHEEL_SIZE + x) * 4 + 1] = lrintf(g * 255.0f);
			hsv_pix[(y * HSV_WHEEL_SIZE + x) * 4 + 2] = lrintf(b * 255.0f);

			if (rad > 1.0f) {
				hsv_pix[(y * HSV_WHEEL_SIZE + x) * 4 + 3] = 0;
			} else {
				hsv_pix[(y * HSV_WHEEL_SIZE + x) * 4 + 3] = 255;
			}
		}
		printf("\n");
	}

	glBindTexture(GL_TEXTURE_2D, HSV_WHEEL);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	check_error();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, HSV_WHEEL_SIZE, HSV_WHEEL_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, hsv_pix);
	check_error();
}

void update_hsv()
{
	hsv2rgb(lift_theta, lift_rad, lift_v, &lift_r, &lift_g, &lift_b);
	hsv2rgb(gamma_theta, gamma_rad, gamma_v * 2.0f, &gamma_r, &gamma_g, &gamma_b);
	hsv2rgb(gain_theta, gain_rad, gain_v * 4.0f, &gain_r, &gain_g, &gain_b);

	if (saturation < 0.0) {
		saturation = 0.0;
	}

	printf("lift: %f %f %f\n", lift_r, lift_g, lift_b);
	printf("gamma: %f %f %f\n", gamma_r, gamma_g, gamma_b);
	printf("gain: %f %f %f\n", gain_r, gain_g, gain_b);
	printf("saturation: %f\n", saturation);
	printf("\n");
}

void read_colorwheel(float xf, float yf, float *rad, float *theta, float *value)
{
	if (xf < 0.2f && yf < 0.2f) {
		float xp = 2.0f * xf / 0.2f - 1.0f;
		float yp = -(2.0f * yf / 0.2f - 1.0f);
		*rad = hypot(xp, yp);
		*theta = atan2(yp, xp);
		if (*rad > 1.0) {
			*rad = 1.0;
		}
	} else if (xf >= 0.22f && xf <= 0.24f) {
		*value = yf / 0.2f;
	}
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
	}

	update_hsv();
}

void load_texture(const char *filename)
{
	SDL_Surface *img = IMG_Load(filename);
	if (img == NULL) {
		fprintf(stderr, "Load of '%s' failed\n", filename);
		exit(1);
	}

	// Convert to RGB.
	SDL_PixelFormat *fmt = img->format;
	SDL_LockSurface(img);
	unsigned char *src_pixels = (unsigned char *)img->pixels;
	unsigned char *dst_pixels = (unsigned char *)malloc(img->w * img->h * 3);
	for (unsigned i = 0; i < img->w * img->h; ++i) {
		unsigned char r, g, b;
		unsigned int temp;
		unsigned int pixel = *(unsigned int *)(src_pixels + i * fmt->BytesPerPixel);

		temp = pixel & fmt->Rmask;
		temp = temp >> fmt->Rshift;
		temp = temp << fmt->Rloss;
		r = temp;

		temp = pixel & fmt->Gmask;
		temp = temp >> fmt->Gshift;
		temp = temp << fmt->Gloss;
		g = temp;

		temp = pixel & fmt->Bmask;
		temp = temp >> fmt->Bshift;
		temp = temp << fmt->Bloss;
		b = temp;

		dst_pixels[i * 3 + 0] = r;
		dst_pixels[i * 3 + 1] = g;
		dst_pixels[i * 3 + 2] = b;
	}
	SDL_UnlockSurface(img);

#if 1
	// we will convert to sRGB in the shader
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, img->w, img->h, 0, GL_RGB, GL_UNSIGNED_BYTE, dst_pixels);
	check_error();
#else
	// implicit sRGB conversion in hardware
	glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, img->w, img->h, 0, GL_RGB, GL_UNSIGNED_BYTE, dst_pixels);
	check_error();
#endif
	free(dst_pixels);
	SDL_FreeSurface(img);
}

void write_ppm(const char *filename, unsigned char *screenbuf)
{
	FILE *fp = fopen(filename, "w");
	fprintf(fp, "P6\n%d %d\n255\n", WIDTH, HEIGHT);
	for (unsigned y = 0; y < HEIGHT; ++y) {
		unsigned char *srcptr = screenbuf + ((HEIGHT - y - 1) * WIDTH) * 4;
		for (unsigned x = 0; x < WIDTH; ++x) {
			fputc(srcptr[x * 4 + 2], fp);
			fputc(srcptr[x * 4 + 1], fp);
			fputc(srcptr[x * 4 + 0], fp);
		}
	}
	fclose(fp);
}

int main(int argc, char **argv)
{
	int quit = 0;

	SDL_Init(SDL_INIT_EVERYTHING);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_SetVideoMode(WIDTH, HEIGHT, 0, SDL_OPENGL);
	SDL_WM_SetCaption("OpenGL window", NULL);
	
	// geez	
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	glBindTexture(GL_TEXTURE_2D, SOURCE_IMAGE);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	check_error();

	load_texture("blg_wheels_woman_1.jpg");
	//glGenerateMipmap(GL_TEXTURE_2D);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 4);
	//check_error();

	//load_texture("maserati_gts_wallpaper_1280x720_01.jpg");
	//load_texture("90630d1295075297-console-games-wallpapers-wallpaper_need_for_speed_prostreet_09_1920x1080.jpg");
	//load_texture("glacier-lake-1280-720-4087.jpg");

#if 0
	// sRGB reverse LUT
	glBindTexture(GL_TEXTURE_1D, SRGB_REVERSE_LUT);
	check_error();
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	check_error();
	float srgb_reverse_tex[4096];
	for (unsigned i = 0; i < 4096; ++i) {
		float x = i / 4095.0;
		if (x < 0.0031308f) {
			srgb_reverse_tex[i] = 12.92f * x;
		} else {
			srgb_reverse_tex[i] = 1.055f * pow(x, 1.0f / 2.4f) - 0.055f;
		}
	}
	glTexImage1D(GL_TEXTURE_1D, 0, GL_LUMINANCE16F_ARB, 4096, 0, GL_LUMINANCE, GL_FLOAT, srgb_reverse_tex);
	check_error();

	// sRGB LUT
	glBindTexture(GL_TEXTURE_1D, SRGB_LUT);
	check_error();
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	check_error();
	float srgb_tex[256];
	for (unsigned i = 0; i < 256; ++i) {
		float x = i / 255.0;
		if (x < 0.04045f) {
			srgb_tex[i] = x * (1.0f / 12.92f);
		} else {
			srgb_tex[i] = pow((x + 0.055) * (1.0 / 1.055f), 2.4);
		}
	}
	glTexImage1D(GL_TEXTURE_1D, 0, GL_LUMINANCE16F_ARB, 256, 0, GL_LUMINANCE, GL_FLOAT, srgb_tex);
	check_error();
#endif

	// generate a PDO to hold the data we read back with glReadPixels()
	// (Intel/DRI goes into a slow path if we don't read to PDO)
	glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 1);
	glBufferData(GL_PIXEL_PACK_BUFFER_ARB, WIDTH * HEIGHT * 4, NULL, GL_STREAM_READ);

	make_hsv_wheel_texture();
	update_hsv();

	int prog = glCreateProgram();
	GLhandleARB vs_obj = read_shader("vs.glsl", GL_VERTEX_SHADER);
	GLhandleARB fs_obj = read_shader("fs.glsl", GL_FRAGMENT_SHADER);
	glAttachObjectARB(prog, vs_obj);
	check_error();
	glAttachObjectARB(prog, fs_obj);
	check_error();
	glLinkProgram(prog);
	check_error();

	GLchar info_log[4096];
	GLsizei log_length = sizeof(info_log) - 1;
	log_length = sizeof(info_log) - 1;
	glGetProgramInfoLog(prog, log_length, &log_length, info_log);
	info_log[log_length] = 0; 
	printf("link: %s\n", info_log);

	struct timespec start, now;
	int frame = 0, screenshot = 0;
	clock_gettime(CLOCK_MONOTONIC, &start);

	while (!quit) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				quit = 1;
			} else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
				quit = 1;
			} else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F1) {
				screenshot = 1;
			} else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
				mouse(event.button.x, event.button.y);
			} else if (event.type == SDL_MOUSEMOTION && (event.motion.state & SDL_BUTTON(1))) {
				mouse(event.motion.x, event.motion.y);
			}
		}

		++frame;

		draw_picture_quad(prog, frame);
		
		glReadPixels(0, 0, WIDTH, HEIGHT, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, BUFFER_OFFSET(0));
		check_error();

		draw_hsv_wheel(0.0f, lift_rad, lift_theta, lift_v);
		draw_hsv_wheel(0.2f, gamma_rad, gamma_theta, gamma_v);
		draw_hsv_wheel(0.4f, gain_rad, gain_theta, gain_v);
		draw_saturation_bar(0.6f);

		SDL_GL_SwapBuffers();
		check_error();

		unsigned char *screenbuf = glMapBuffer(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY);
		check_error();
		if (screenshot) {
			char filename[256];
			sprintf(filename, "frame%05d.ppm", frame);
			write_ppm(filename, screenbuf);
			printf("Screenshot: %s\n", filename);
			screenshot = 0;
		}
		glUnmapBuffer(GL_PIXEL_PACK_BUFFER_ARB);
		check_error();

#if 1
		clock_gettime(CLOCK_MONOTONIC, &now);
		double elapsed = now.tv_sec - start.tv_sec +
			1e-9 * (now.tv_nsec - start.tv_nsec);
		printf("%d frames in %.3f seconds = %.1f fps (%.1f ms/frame)\n",
			frame, elapsed, frame / elapsed,
			1e3 * elapsed / frame);
#endif
	}
	return 0; 
}
