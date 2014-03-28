#ifdef GL_ES
precision highp float;
#endif

#ifdef GL_EXT_gpu_shader4
// We sometimes want round().
#extension GL_EXT_gpu_shader4 : enable
#endif

varying vec2 tc;

vec4 tex2D(sampler2D s, vec2 coord)
{
	return texture2D(s, coord);
}
