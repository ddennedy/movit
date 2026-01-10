// It's actually (but surprisingly) not correct to do a mix() here;
// it would be if we had postmultiplied alpha and didn't have to worry
// about alpha in the bottom layer, but given that we use premultiplied
// alpha all over, top shouldn't actually be multiplied by anything.
//
// These formulas come from Wikipedia:
//
//   http://en.wikipedia.org/wiki/Alpha_compositing
//
// We use the associative version given. However, note that since we want
// _output_ to be premultiplied, C_o from Wikipedia is not what we want,
// but rather c_o (which is not explicitly given, but obviously is just
// C_o without the division by alpha_o).
//
// Blend mode implementations based on Qt QPainter::CompositionMode
// Formulas derived from Qt's qcompositionfunctions.cpp
// Note: All inputs use premultiplied alpha (Sca, Dca format where color * alpha)

// Prevent redefinition when overlay effect is used multiple times
#ifndef OVERLAY_BLEND_FUNCTIONS
#define OVERLAY_BLEND_FUNCTIONS

// Helper function for alpha blending
float mix_alpha(float da, float sa) {
	return sa + da - sa * da;
}

// Porter-Duff blend modes (premultiplied alpha versions)

vec4 blend_clear(vec4 bottom, vec4 top) {
	return vec4(0.0, 0.0, 0.0, 0.0);
}

vec4 blend_source(vec4 bottom, vec4 top) {
	return top;
}

vec4 blend_destination(vec4 bottom, vec4 top) {
	return bottom;
}

vec4 blend_source_over(vec4 bottom, vec4 top) {
	// Sca + Dca * (1 - Sa)
	return top + bottom * (1.0 - top.a);
}

vec4 blend_destination_over(vec4 bottom, vec4 top) {
	// Dca + Sca * (1 - Da)
	return bottom + top * (1.0 - bottom.a);
}

vec4 blend_source_in(vec4 bottom, vec4 top) {
	// Sca * Da
	return vec4(top.rgb * bottom.a, top.a * bottom.a);
}

vec4 blend_destination_in(vec4 bottom, vec4 top) {
	// Dca * Sa
	return vec4(bottom.rgb * top.a, bottom.a * top.a);
}

vec4 blend_source_out(vec4 bottom, vec4 top) {
	// Sca * (1 - Da)
	return vec4(top.rgb * (1.0 - bottom.a), top.a * (1.0 - bottom.a));
}

vec4 blend_destination_out(vec4 bottom, vec4 top) {
	// Dca * (1 - Sa)
	return vec4(bottom.rgb * (1.0 - top.a), bottom.a * (1.0 - top.a));
}

vec4 blend_source_atop(vec4 bottom, vec4 top) {
	// Sca * Da + Dca * (1 - Sa)
	return vec4(top.rgb * bottom.a + bottom.rgb * (1.0 - top.a), bottom.a);
}

vec4 blend_destination_atop(vec4 bottom, vec4 top) {
	// Dca * Sa + Sca * (1 - Da)
	return vec4(bottom.rgb * top.a + top.rgb * (1.0 - bottom.a), top.a);
}

vec4 blend_xor(vec4 bottom, vec4 top) {
	// Sca * (1 - Da) + Dca * (1 - Sa)
	return top * (1.0 - bottom.a) + bottom * (1.0 - top.a);
}

// SVG 1.2 blend modes

vec4 blend_plus(vec4 bottom, vec4 top) {
	// Simply add the colors and clamp
	return min(bottom + top, vec4(1.0));
}

vec4 blend_multiply(vec4 bottom, vec4 top) {
	float da = bottom.a;
	float sa = top.a;
	vec3 result;
	
	// Dca' = Sca.Dca + Sca.(1 - Da) + Dca.(1 - Sa)
	result.r = top.r * bottom.r + top.r * (1.0 - da) + bottom.r * (1.0 - sa);
	result.g = top.g * bottom.g + top.g * (1.0 - da) + bottom.g * (1.0 - sa);
	result.b = top.b * bottom.b + top.b * (1.0 - da) + bottom.b * (1.0 - sa);
	
	return vec4(result, mix_alpha(da, sa));
}

vec4 blend_screen(vec4 bottom, vec4 top) {
	float da = bottom.a;
	float sa = top.a;
	vec3 result;
	
	// Dca' = Sca + Dca - Sca.Dca
	result.r = top.r + bottom.r - top.r * bottom.r;
	result.g = top.g + bottom.g - top.g * bottom.g;
	result.b = top.b + bottom.b - top.b * bottom.b;
	
	return vec4(result, mix_alpha(da, sa));
}

vec4 blend_overlay(vec4 bottom, vec4 top) {
	float da = bottom.a;
	float sa = top.a;
	vec3 result;
	
	// For each channel:
	// if 2.Dca < Da
	//     Dca' = 2.Sca.Dca + Sca.(1 - Da) + Dca.(1 - Sa)
	// otherwise
	//     Dca' = Sa.Da - 2.(Da - Dca).(Sa - Sca) + Sca.(1 - Da) + Dca.(1 - Sa)
	
	float temp_r = top.r * (1.0 - da) + bottom.r * (1.0 - sa);
	float temp_g = top.g * (1.0 - da) + bottom.g * (1.0 - sa);
	float temp_b = top.b * (1.0 - da) + bottom.b * (1.0 - sa);
	
	if (2.0 * bottom.r < da)
		result.r = 2.0 * top.r * bottom.r + temp_r;
	else
		result.r = sa * da - 2.0 * (da - bottom.r) * (sa - top.r) + temp_r;
	
	if (2.0 * bottom.g < da)
		result.g = 2.0 * top.g * bottom.g + temp_g;
	else
		result.g = sa * da - 2.0 * (da - bottom.g) * (sa - top.g) + temp_g;
	
	if (2.0 * bottom.b < da)
		result.b = 2.0 * top.b * bottom.b + temp_b;
	else
		result.b = sa * da - 2.0 * (da - bottom.b) * (sa - top.b) + temp_b;
	
	return vec4(result, mix_alpha(da, sa));
}

vec4 blend_darken(vec4 bottom, vec4 top) {
	float da = bottom.a;
	float sa = top.a;
	vec3 result;
	
	// Dca' = min(Sca.Da, Dca.Sa) + Sca.(1 - Da) + Dca.(1 - Sa)
	result.r = min(top.r * da, bottom.r * sa) + top.r * (1.0 - da) + bottom.r * (1.0 - sa);
	result.g = min(top.g * da, bottom.g * sa) + top.g * (1.0 - da) + bottom.g * (1.0 - sa);
	result.b = min(top.b * da, bottom.b * sa) + top.b * (1.0 - da) + bottom.b * (1.0 - sa);
	
	return vec4(result, mix_alpha(da, sa));
}

vec4 blend_lighten(vec4 bottom, vec4 top) {
	float da = bottom.a;
	float sa = top.a;
	vec3 result;
	
	// Dca' = max(Sca.Da, Dca.Sa) + Sca.(1 - Da) + Dca.(1 - Sa)
	result.r = max(top.r * da, bottom.r * sa) + top.r * (1.0 - da) + bottom.r * (1.0 - sa);
	result.g = max(top.g * da, bottom.g * sa) + top.g * (1.0 - da) + bottom.g * (1.0 - sa);
	result.b = max(top.b * da, bottom.b * sa) + top.b * (1.0 - da) + bottom.b * (1.0 - sa);
	
	return vec4(result, mix_alpha(da, sa));
}

vec4 blend_color_dodge(vec4 bottom, vec4 top) {
	float da = bottom.a;
	float sa = top.a;
	vec3 result;
	
	// if Sca.Da + Dca.Sa >= Sa.Da
	//     Dca' = Sa.Da + Sca.(1 - Da) + Dca.(1 - Sa)
	// otherwise if Sca == Sa
	//     Dca' = Dca.Sa + Sca.(1 - Da) + Dca.(1 - Sa)
	// otherwise
	//     Dca' = Dca.Sa/(1 - Sca/Sa) + Sca.(1 - Da) + Dca.(1 - Sa)
	
	#define COLOR_DODGE(dst, src) \
		(src * da + dst * sa >= sa * da) ? (sa * da + src * (1.0 - da) + dst * (1.0 - sa)) : \
		((src >= sa - 0.001) ? (dst * sa + src * (1.0 - da) + dst * (1.0 - sa)) : \
		(dst * sa / max(1.0 - src / max(sa, 0.001), 0.001) + src * (1.0 - da) + dst * (1.0 - sa)))
	
	result.r = COLOR_DODGE(bottom.r, top.r);
	result.g = COLOR_DODGE(bottom.g, top.g);
	result.b = COLOR_DODGE(bottom.b, top.b);
	#undef COLOR_DODGE
	
	return vec4(result, mix_alpha(da, sa));
}

vec4 blend_color_burn(vec4 bottom, vec4 top) {
	float da = bottom.a;
	float sa = top.a;
	vec3 result;
	
	// if Sca.Da + Dca.Sa <= Sa.Da
	//     Dca' = Sca.(1 - Da) + Dca.(1 - Sa)
	// otherwise if Sca == 0
	//     Dca' = Dca.Sa + Sca.(1 - Da) + Dca.(1 - Sa)
	// otherwise
	//     Dca' = Sa.Da - (Da - Dca).Sa/Sca + Sca.(1 - Da) + Dca.(1 - Sa)
	
	#define COLOR_BURN(dst, src) \
		(src * da + dst * sa <= sa * da) ? (src * (1.0 - da) + dst * (1.0 - sa)) : \
		((src <= 0.001) ? (dst * sa + src * (1.0 - da) + dst * (1.0 - sa)) : \
		(sa * da - (da - dst) * sa / max(src, 0.001) + src * (1.0 - da) + dst * (1.0 - sa)))
	
	result.r = COLOR_BURN(bottom.r, top.r);
	result.g = COLOR_BURN(bottom.g, top.g);
	result.b = COLOR_BURN(bottom.b, top.b);
	#undef COLOR_BURN
	
	return vec4(result, mix_alpha(da, sa));
}

vec4 blend_hard_light(vec4 bottom, vec4 top) {
	float da = bottom.a;
	float sa = top.a;
	vec3 result;
	
	// if 2.Sca < Sa
	//     Dca' = 2.Sca.Dca + Sca.(1 - Da) + Dca.(1 - Sa)
	// otherwise
	//     Dca' = Sa.Da - 2.(Da - Dca).(Sa - Sca) + Sca.(1 - Da) + Dca.(1 - Sa)
	
	float temp_r = top.r * (1.0 - da) + bottom.r * (1.0 - sa);
	float temp_g = top.g * (1.0 - da) + bottom.g * (1.0 - sa);
	float temp_b = top.b * (1.0 - da) + bottom.b * (1.0 - sa);
	
	if (2.0 * top.r < sa)
		result.r = 2.0 * top.r * bottom.r + temp_r;
	else
		result.r = sa * da - 2.0 * (da - bottom.r) * (sa - top.r) + temp_r;
	
	if (2.0 * top.g < sa)
		result.g = 2.0 * top.g * bottom.g + temp_g;
	else
		result.g = sa * da - 2.0 * (da - bottom.g) * (sa - top.g) + temp_g;
	
	if (2.0 * top.b < sa)
		result.b = 2.0 * top.b * bottom.b + temp_b;
	else
		result.b = sa * da - 2.0 * (da - bottom.b) * (sa - top.b) + temp_b;
	
	return vec4(result, mix_alpha(da, sa));
}

vec4 blend_soft_light(vec4 bottom, vec4 top) {
	float da = bottom.a;
	float sa = top.a;
	vec3 result;
	
	// Complex soft light formula
	#define SOFT_LIGHT(dst, src) { \
		float dst_np = (da > 0.001) ? dst / da : 0.0; \
		float temp = src * (1.0 - da) + dst * (1.0 - sa); \
		if (2.0 * src < sa) { \
			result_val = (dst * (sa + (2.0 * src - sa) * (1.0 - dst_np)) + temp); \
		} else if (4.0 * dst <= da) { \
			float dst_sq = dst_np * dst_np; \
			float dst_cb = dst_sq * dst_np; \
			result_val = (dst * sa + da * (2.0 * src - sa) * ((16.0 * dst_cb - 12.0 * dst_sq + 3.0 * dst_np)) + temp); \
		} else { \
			result_val = (dst * sa + da * (2.0 * src - sa) * (sqrt(dst_np) - dst_np) + temp); \
		} \
	}
	
	float result_val;
	SOFT_LIGHT(bottom.r, top.r)
	result.r = result_val;
	SOFT_LIGHT(bottom.g, top.g)
	result.g = result_val;
	SOFT_LIGHT(bottom.b, top.b)
	result.b = result_val;
	#undef SOFT_LIGHT
	
	return vec4(result, mix_alpha(da, sa));
}

vec4 blend_difference(vec4 bottom, vec4 top) {
	float da = bottom.a;
	float sa = top.a;
	vec3 result;
	
	// Dca' = abs(Dca.Sa - Sca.Da) + Sca.(1 - Da) + Dca.(1 - Sa)
	//      = Sca + Dca - 2.min(Sca.Da, Dca.Sa)
	result.r = top.r + bottom.r - 2.0 * min(top.r * da, bottom.r * sa);
	result.g = top.g + bottom.g - 2.0 * min(top.g * da, bottom.g * sa);
	result.b = top.b + bottom.b - 2.0 * min(top.b * da, bottom.b * sa);
	
	return vec4(result, mix_alpha(da, sa));
}

vec4 blend_exclusion(vec4 bottom, vec4 top) {
	float da = bottom.a;
	float sa = top.a;
	vec3 result;
	
	// Dca' = (Sca.Da + Dca.Sa - 2.Sca.Dca) + Sca.(1 - Da) + Dca.(1 - Sa)
	//      = Sca + Dca - 2.Sca.Dca
	result.r = top.r + bottom.r - 2.0 * top.r * bottom.r;
	result.g = top.g + bottom.g - 2.0 * top.g * bottom.g;
	result.b = top.b + bottom.b - 2.0 * top.b * bottom.b;
	
	return vec4(result, mix_alpha(da, sa));
}

#endif // OVERLAY_BLEND_FUNCTIONS

vec4 FUNCNAME(vec2 tc) {
// SWAP_INPUTS will be #defined to 1 if we want to swap the two inputs,
#if SWAP_INPUTS
	vec4 bottom = INPUT2(tc);
	vec4 top = INPUT1(tc);
#else
	vec4 bottom = INPUT1(tc);
	vec4 top = INPUT2(tc);
#endif

	switch (PREFIX(blend_mode)) {
	case 0:
		return blend_source_over(bottom, top);
	case 1:
		return blend_destination_over(bottom, top);
	case 2:
		return blend_clear(bottom, top);
	case 3:
		return blend_source(bottom, top);
	case 4:
		return blend_destination(bottom, top);
	case 5:
		return blend_source_in(bottom, top);
	case 6:
		return blend_destination_in(bottom, top);
	case 7:
		return blend_source_out(bottom, top);
	case 8:
		return blend_destination_out(bottom, top);
	case 9:
		return blend_source_atop(bottom, top);
	case 10:
		return blend_destination_atop(bottom, top);
	case 11:
		return blend_xor(bottom, top);
	case 12:
		return blend_plus(bottom, top);
	case 13:
		return blend_multiply(bottom, top);
	case 14:
		return blend_screen(bottom, top);
	case 15:
		return blend_overlay(bottom, top);
	case 16:
		return blend_darken(bottom, top);
	case 17:
		return blend_lighten(bottom, top);
	case 18:
		return blend_color_dodge(bottom, top);
	case 19:
		return blend_color_burn(bottom, top);
	case 20:
		return blend_hard_light(bottom, top);
	case 21:
		return blend_soft_light(bottom, top);
	case 22:
		return blend_difference(bottom, top);
	case 23:
		return blend_exclusion(bottom, top);
	default:
		return blend_source_over(bottom, top);
	}
}

#undef SWAP_INPUTS

