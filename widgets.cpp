#include <epoxy/gl.h>
#include <math.h>

#include "widgets.h"
#include "util.h"

#define HSV_WHEEL_SIZE 128

namespace movit {

GLuint hsv_wheel_num;

void draw_hsv_wheel(float y, float rad, float theta, float value)
{
	glUseProgram(0);
	check_error();
	glActiveTexture(GL_TEXTURE0);
	check_error();
	glEnable(GL_TEXTURE_2D);
	check_error();
	glBindTexture(GL_TEXTURE_2D, hsv_wheel_num);
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

void draw_saturation_bar(float y, float saturation)
{
	glUseProgram(0);
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
	glVertex2f(0.2f * saturation * 9.0f / 16.0f, y + 0.01f);
	glEnd();

	glColor3f(1.0f, 1.0f, 1.0f);
}

void make_hsv_wheel_texture()
{
	glGenTextures(1, &hsv_wheel_num);

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
	}

	glBindTexture(GL_TEXTURE_2D, hsv_wheel_num);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	check_error();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, HSV_WHEEL_SIZE, HSV_WHEEL_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, hsv_pix);
	check_error();
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


}  // namespace movit
