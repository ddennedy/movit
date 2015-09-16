void main()
{
	vec4 color = INPUT(tc);
#if YCBCR_OUTPUT_PLANAR
	gl_FragData[0] = color.rrra;
	gl_FragData[1] = color.ggga;
	gl_FragData[2] = color.bbba;
#elif YCBCR_OUTPUT_SPLIT_Y_AND_CBCR
	gl_FragData[0] = color.rrra;
	gl_FragData[1] = color.gbba;
#else
	gl_FragColor = color;
#endif
}
