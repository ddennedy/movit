#ifndef _WIDGETS_H
#define _WIDGETS_H 1

// Some simple UI widgets for test use.

void draw_hsv_wheel(float y, float rad, float theta, float value);
void draw_saturation_bar(float y, float saturation);
void make_hsv_wheel_texture();
void read_colorwheel(float xf, float yf, float *rad, float *theta, float *value);

#endif // !defined(_WIDGETS_H)
