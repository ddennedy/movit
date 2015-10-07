#ifndef _MOVIT_WIDGETS_H
#define _MOVIT_WIDGETS_H 1

// Some simple UI widgets for test use.

namespace movit {

void draw_hsv_wheel(float y, float rad, float theta, float value);
void draw_saturation_bar(float y, float saturation);
void init_hsv_resources();
void cleanup_hsv_resources();
void read_colorwheel(float xf, float yf, float *rad, float *theta, float *value);

}  // namespace movit

#endif // !defined(_MOVIT_WIDGETS_H)
