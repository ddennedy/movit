#include <GL/glew.h>
#include <string>

#include "init.h"
#include "util.h"

bool movit_initialized = false;
MovitDebugLevel movit_debug_level = MOVIT_DEBUG_ON;
float movit_texel_subpixel_precision;
bool movit_srgb_textures_supported;

// The rules for objects with nontrivial constructors in static scope
// are somewhat convoluted, and easy to mess up. We simply have a
// pointer instead (and never care to clean it up).
std::string *movit_data_directory = NULL;

namespace {

void measure_texel_subpixel_precision()
{
	static const unsigned width = 4096;

	// Generate a destination texture to render to, and an FBO.
	GLuint dst_texnum, fbo;

	glGenTextures(1, &dst_texnum);
	check_error();
	glBindTexture(GL_TEXTURE_2D, dst_texnum);
	check_error();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, width, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	check_error();

	glGenFramebuffers(1, &fbo);
	check_error();
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	check_error();
	glFramebufferTexture2D(
		GL_FRAMEBUFFER,
		GL_COLOR_ATTACHMENT0,
		GL_TEXTURE_2D,
		dst_texnum,
		0);
	check_error();

	// Now generate a simple texture that's just [0,1].
	GLuint src_texnum;
	float texdata[] = { 0, 1 };
	glGenTextures(1, &dst_texnum);
	check_error();
	glBindTexture(GL_TEXTURE_1D, dst_texnum);
	check_error();
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	check_error();
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	check_error();
	glTexImage1D(GL_TEXTURE_1D, 0, GL_LUMINANCE16F_ARB, 2, 0, GL_LUMINANCE, GL_FLOAT, texdata);
	check_error();
	glEnable(GL_TEXTURE_1D);
	check_error();

	// Basic state.
	glDisable(GL_BLEND);
	check_error();
	glDisable(GL_DEPTH_TEST);
	check_error();
	glDepthMask(GL_FALSE);
	check_error();

	glViewport(0, 0, width, 1);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, 1.0, 0.0, 1.0, 0.0, 1.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	check_error();

	// Draw the texture stretched over a long quad, interpolating it out.
	// Note that since the texel center is in (0.5), we need to adjust the
	// texture coordinates in order not to get long stretches of (1,1,1,...)
	// at the start and (...,0,0,0) at the end.
	glBegin(GL_QUADS);

	glTexCoord1f(0.25f);
	glVertex2f(0.0f, 0.0f);

	glTexCoord1f(0.75f);
	glVertex2f(1.0f, 0.0f);

	glTexCoord1f(0.75f);
	glVertex2f(1.0f, 1.0f);

	glTexCoord1f(0.25f);
	glVertex2f(0.0f, 1.0f);

	glEnd();
	check_error();

	glDisable(GL_TEXTURE_1D);
	check_error();

	// Now read the data back and see what the card did.
	// (We only look at the red channel; the others will surely be the same.)
	// We assume a linear ramp; anything else will give sort of odd results here.
	float out_data[width];
	glReadPixels(0, 0, width, 1, GL_RED, GL_FLOAT, out_data);
	check_error();

	float biggest_jump = 0.0f;
	for (unsigned i = 1; i < width; ++i) {
		assert(out_data[i] >= out_data[i - 1]);
		biggest_jump = std::max(biggest_jump, out_data[i] - out_data[i - 1]);
	}

	movit_texel_subpixel_precision = biggest_jump;

	// Clean up.
	glBindTexture(GL_TEXTURE_1D, 0);
	check_error();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	check_error();
	glDeleteFramebuffers(1, &fbo);
	check_error();
	glDeleteTextures(1, &dst_texnum);
	check_error();
	glDeleteTextures(1, &src_texnum);
	check_error();
}

void check_extensions()
{
	// We fundamentally need FBOs and floating-point textures.
	assert(glewIsSupported("GL_ARB_framebuffer_object") != 0);
	assert(glewIsSupported("GL_ARB_texture_float") != 0);

	// We assume that we can use non-power-of-two textures without restrictions.
	assert(glewIsSupported("GL_ARB_texture_non_power_of_two") != 0);

	// We also need GLSL fragment shaders.
	assert(glewIsSupported("GL_ARB_fragment_shader") != 0);
	assert(glewIsSupported("GL_ARB_shading_language_100") != 0);

	// FlatInput and YCbCrInput uses PBOs. (They could in theory do without,
	// but no modern card would really not provide it.)
	assert(glewIsSupported("GL_ARB_pixel_buffer_object") != 0);

	// ResampleEffect uses RG textures to encode a two-component LUT.
	assert(glewIsSupported("GL_ARB_texture_rg") != 0);

	// sRGB texture decode would be nice, but are not mandatory
	// (GammaExpansionEffect can do the same thing if needed).
	movit_srgb_textures_supported = glewIsSupported("GL_EXT_texture_sRGB");
}

}  // namespace

void init_movit(const std::string& data_directory, MovitDebugLevel debug_level)
{
	if (movit_initialized) {
		return;
	}

	movit_data_directory = new std::string(data_directory);
	movit_debug_level = debug_level;

	glewInit();

	// geez	
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	measure_texel_subpixel_precision();
	check_extensions();

	movit_initialized = true;
}
