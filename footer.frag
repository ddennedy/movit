// GLSL is pickier than the C++ preprocessor in if-testing for undefined
// tokens; do some fixups here to keep it happy.

#ifndef YCBCR_OUTPUT_PLANAR
#define YCBCR_OUTPUT_PLANAR 0
#endif

#ifndef YCBCR_OUTPUT_SPLIT_Y_AND_CBCR
#define YCBCR_OUTPUT_SPLIT_Y_AND_CBCR 0
#endif

#ifndef SECOND_YCBCR_OUTPUT_PLANAR
#define SECOND_YCBCR_OUTPUT_PLANAR 0
#endif

#ifndef SECOND_YCBCR_OUTPUT_SPLIT_Y_AND_CBCR
#define SECOND_YCBCR_OUTPUT_SPLIT_Y_AND_CBCR 0
#endif

#ifndef SECOND_YCBCR_OUTPUT_INTERLEAVED
#define SECOND_YCBCR_OUTPUT_INTERLEAVED 0
#endif

#ifndef YCBCR_ALSO_OUTPUT_RGBA
#define YCBCR_ALSO_OUTPUT_RGBA 0
#endif

#ifndef SQUARE_ROOT_TRANSFORMATION
#define SQUARE_ROOT_TRANSFORMATION 0
#endif

#if YCBCR_OUTPUT_PLANAR
out vec4 Y, Cb, Cr;
#elif YCBCR_OUTPUT_SPLIT_Y_AND_CBCR
out vec4 Y, Chroma;
#else
out vec4 FragColor;  // Y'CbCr or RGBA.
#endif

#if SECOND_YCBCR_OUTPUT_PLANAR
out vec4 Y2, Cb2, Cr2;
#elif SECOND_YCBCR_OUTPUT_SPLIT_Y_AND_CBCR
out vec4 Y2, Chroma2;
#elif SECOND_YCBCR_OUTPUT_INTERLEAVED
out vec4 YCbCr2;
#endif

#if YCBCR_ALSO_OUTPUT_RGBA
out vec4 RGBA;
#endif

void main()
{
#if YCBCR_ALSO_OUTPUT_RGBA
	vec4 color[2] = INPUT(tc);
	vec4 color0 = color[0];
	vec4 color1 = color[1];
#else
	vec4 color0 = INPUT(tc);
#endif

#if SQUARE_ROOT_TRANSFORMATION
	// Make sure we don't give negative values to sqrt.
	color0.rgb = sqrt(max(color0.rgb, 0.0));
#endif

#if YCBCR_OUTPUT_PLANAR
	Y = color0.rrra;
	Cb = color0.ggga;
	Cr = color0.bbba;
#elif YCBCR_OUTPUT_SPLIT_Y_AND_CBCR
	Y = color0.rrra;
	Chroma = color0.gbba;
#else
	FragColor = color0;
#endif

	// Exactly the same, just with other outputs.
	// (GLSL does not allow arrays of outputs.)
#if SECOND_YCBCR_OUTPUT_PLANAR
	Y2 = color0.rrra;
	Cb2 = color0.ggga;
	Cr2 = color0.bbba;
#elif SECOND_YCBCR_OUTPUT_SPLIT_Y_AND_CBCR
	Y2 = color0.rrra;
	Chroma2 = color0.gbba;
#elif SECOND_YCBCR_OUTPUT_INTERLEAVED
	YCbCr2 = color0;
#endif

#if YCBCR_ALSO_OUTPUT_RGBA
	RGBA = color1;
#endif
}
