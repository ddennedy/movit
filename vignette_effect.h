#ifndef _VIGNETTE_EFFECT_H
#define _VIGNETTE_EFFECT_H 1

#include "effect.h"

class VignetteEffect : public Effect {
public:
	VignetteEffect();
	std::string output_vertex_shader();
	std::string output_fragment_shader();

	void set_uniforms(GLuint glsl_program_num, const std::string &prefix);

private:
	Point2D center;
	float radius, inner_radius;
};

#endif // !defined(_VIGNETTE_EFFECT_H)
