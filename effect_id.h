#ifndef _EFFECT_ID_H
#define _EFFECT_ID_H 1

enum EffectId {
       // Mostly for internal use.
       EFFECT_GAMMA_EXPANSION = 0,
       EFFECT_GAMMA_COMPRESSION,
       EFFECT_RGB_PRIMARIES_CONVERSION,

       // Color.
       EFFECT_LIFT_GAMMA_GAIN,
       EFFECT_SATURATION,
};

#endif // !defined(_EFFECT_ID_H)
