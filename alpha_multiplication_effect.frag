vec4 FUNCNAME(vec2 tc) {
	vec4 x = INPUT(tc);
	x.rgb *= x.aaa;	
	return x;
}
