#if YCBCR_OUTPUT_PLANAR
out vec4 Y;
out vec4 Cb;
out vec4 Cr;
#elif YCBCR_OUTPUT_SPLIT_Y_AND_CBCR
out vec4 Y;
out vec4 Chroma;
#else
out vec4 FragColor;
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

#if YCBCR_ALSO_OUTPUT_RGBA
	RGBA = color1;
#endif
}
