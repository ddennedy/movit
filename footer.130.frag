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

void main()
{
	vec4 color = INPUT(tc);
#if YCBCR_OUTPUT_PLANAR
	Y = color.rrra;
	Cb = color.ggga;
	Cr = color.bbba;
#elif YCBCR_OUTPUT_SPLIT_Y_AND_CBCR
	Y = color.rrra;
	Chroma = color.gbba;
#else
	FragColor = color;
#endif
}
