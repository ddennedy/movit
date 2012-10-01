#ifndef _EFFECT_ID_H
#define _EFFECT_ID_H 1

enum EffectId {
       // Mostly for internal use.
       GAMMA_CONVERSION = 0,
       RGB_PRIMARIES_CONVERSION,

       // Color.
       LIFT_GAMMA_GAIN,
       SATURATION,
};

#endif // !defined(_EFFECT_ID_H)
