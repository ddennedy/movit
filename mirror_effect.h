#ifndef _MIRROR_EFFECT_H
#define _MIRROR_EFFECT_H 1

#include "effect.h"

class MirrorEffect : public Effect {
public:
	MirrorEffect();
	std::string output_fragment_shader();
};

#endif // !defined(_MIRROR_EFFECT_H)
