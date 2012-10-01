#version 120
varying vec2 tc;
//varying vec3 lift, inv_gamma, gain;
//uniform vec3 gamma;
//varying vec3 inv_gamma;
//varying mat3 colorMat;
//uniform vec3 lift, gain;

vec3 to_linear(vec3 x) {
	vec3 a = x * vec3(1.0/12.92); 
	vec3 b = pow((x + vec3(0.055)) * vec3(1.0/1.055), vec3(2.4));
	vec3 f = vec3(greaterThan(x, vec3(0.04045)));
	return mix(a, b, f); 
} 

void main()
{
        tc = gl_MultiTexCoord0.st;

	//lift = to_linear(vec3(rgba.r, 0.0, 0.0));
	//lift = vec3(rgba.r, 0.0, 0.0);
//	lift = vec3(0.0, 0.0, 0.0);
//	inv_gamma = vec3(1.0) / vec3(1.0, 1.0, 1.0 + 2.0 * rgba.r);
//	gain = vec3(1.0, 1.0, 1.0);
	//gain = vec3(1.0 + rgba.r * 3.0, 1.0, 1.0);
	//gain = vec3(1.0, 1.0, 1.0 + rgba.r * 3.0);
	//inv_gamma = vec3(1.0) / gamma;

#if 0
        vec4 rgba = gl_MultiTexCoord1;
	rgba.b = rgba.r;
	rgba.r = 1.0f;
	rgba.g = 1.0f;
	rgba.a = 0.0f;

	mat3 rgb_to_xyz = mat3(
		0.4124, 0.3576, 0.1805,
		0.2126, 0.7152, 0.0722,
		0.0193, 0.1192, 0.9505
	);
	mat3 xyz_to_lms = mat3(
		 0.4002, 0.7076, -0.0808,
		-0.2263, 1.1653,  0.0457,
		    0.0,    0.0,  0.9182
	);
	mat3 lms_corr_matrix = mat3(
		rgba.r, 0, 0,
		0, rgba.g, 0,
		0, 0, rgba.b
	);
	mat3 lms_to_xyz = mat3(
		1.86007, -1.12948,  0.21990,
		0.36122,  0.63880, -0.00001,
		0.00000,  0.00000,  1.08909
	);
	mat3 xyz_to_rgb = mat3(
	         3.240625, -1.537208, -0.498629,
		-0.968931,  1.875756,  0.041518,
		 0.055710, -0.204021,  1.056996
	);
	colorMat = xyz_to_rgb * lms_to_xyz * lms_corr_matrix * xyz_to_lms * rgb_to_xyz;
#endif

        gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}

