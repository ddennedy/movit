#include <epoxy/gl.h>
#include <assert.h>
#include <stddef.h>
#include <algorithm>
#include <string>

#include "init.h"
#include "resource_pool.h"
#include "util.h"

using namespace std;

namespace movit {

bool movit_initialized = false;
MovitDebugLevel movit_debug_level = MOVIT_DEBUG_ON;
float movit_texel_subpixel_precision;
bool movit_timer_queries_supported, movit_compute_shaders_supported;
int movit_num_wrongly_rounded;
MovitShaderModel movit_shader_model;

// The rules for objects with nontrivial constructors in static scope
// are somewhat convoluted, and easy to mess up. We simply have a
// pointer instead (and never care to clean it up).
string *movit_data_directory = nullptr;

namespace {

void measure_texel_subpixel_precision()
{
	ResourcePool resource_pool;
	static const unsigned width = 4096;

	// Generate a destination texture to render to, and an FBO.
	GLuint dst_texnum, fbo;

	glGenTextures(1, &dst_texnum);
	check_error();
	glBindTexture(GL_TEXTURE_2D, dst_texnum);
	check_error();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, 1, 0, GL_RGBA, GL_FLOAT, nullptr);
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
	glGenTextures(1, &src_texnum);
	check_error();
	glBindTexture(GL_TEXTURE_2D, src_texnum);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	check_error();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, 2, 1, 0, GL_RED, GL_FLOAT, texdata);
	check_error();

	// Basic state.
	glDisable(GL_BLEND);
	check_error();
	glDisable(GL_DEPTH_TEST);
	check_error();
	glDepthMask(GL_FALSE);
	check_error();

	glViewport(0, 0, width, 1);

	vector<string> frag_shader_outputs;
	GLuint glsl_program_num = resource_pool.compile_glsl_program(
		read_version_dependent_file("vs", "vert"),
		read_version_dependent_file("texture1d", "frag"),
		frag_shader_outputs);
	glUseProgram(glsl_program_num);
	check_error();
	glUniform1i(glGetUniformLocation(glsl_program_num, "tex"), 0);  // Bind the 2D sampler.
	check_error();

	// Draw the texture stretched over a long quad, interpolating it out.
	// Note that since the texel center is in (0.5), we need to adjust the
	// texture coordinates in order not to get long stretches of (1,1,1,...)
	// at the start and (...,0,0,0) at the end.
	float vertices[] = {
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 1.0f,
		1.0f, 0.0f
	};
	float texcoords[] = {
		0.25f, 0.0f,
		0.25f, 0.0f,
		0.75f, 0.0f,
		0.75f, 0.0f
	};

	GLuint vao;
	glGenVertexArrays(1, &vao);
	check_error();
	glBindVertexArray(vao);
	check_error();

	GLuint position_vbo = fill_vertex_attribute(glsl_program_num, "position", 2, GL_FLOAT, sizeof(vertices), vertices);
	GLuint texcoord_vbo = fill_vertex_attribute(glsl_program_num, "texcoord", 2, GL_FLOAT, sizeof(texcoords), texcoords);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	check_error();

	cleanup_vertex_attribute(glsl_program_num, "position", position_vbo);
	cleanup_vertex_attribute(glsl_program_num, "texcoord", texcoord_vbo);

	glUseProgram(0);
	check_error();

	// Now read the data back and see what the card did.
	// (We only look at the red channel; the others will surely be the same.)
	// We assume a linear ramp; anything else will give sort of odd results here.
	float out_data[width * 4];
	glReadPixels(0, 0, width, 1, GL_RGBA, GL_FLOAT, out_data);
	check_error();

	float biggest_jump = 0.0f;
	for (unsigned i = 1; i < width; ++i) {
		assert(out_data[i * 4] >= out_data[(i - 1) * 4]);
		biggest_jump = max(biggest_jump, out_data[i * 4] - out_data[(i - 1) * 4]);
	}

	assert(biggest_jump > 0.0);
	movit_texel_subpixel_precision = biggest_jump;

	// Clean up.
	glBindTexture(GL_TEXTURE_2D, 0);
	check_error();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	check_error();
	glDeleteFramebuffers(1, &fbo);
	check_error();
	glDeleteTextures(1, &dst_texnum);
	check_error();
	glDeleteTextures(1, &src_texnum);
	check_error();

	resource_pool.release_glsl_program(glsl_program_num);
	glDeleteVertexArrays(1, &vao);
	check_error();
}

void measure_roundoff_problems()
{
	ResourcePool resource_pool;

	// Generate a destination texture to render to, and an FBO.
	GLuint dst_texnum, fbo;

	glGenTextures(1, &dst_texnum);
	check_error();
	glBindTexture(GL_TEXTURE_2D, dst_texnum);
	check_error();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 512, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
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

	// Now generate a texture where every value except the last should be
	// rounded up to the next one. However, there are cards (in highly
	// common use) that can't do this right, for unknown reasons.
	GLuint src_texnum;
	float texdata[512];
	for (int i = 0; i < 256; ++i) {
		texdata[i * 2 + 0] = (i + 0.48) / 255.0;
		texdata[i * 2 + 1] = (i + 0.52) / 255.0;
	}
	glGenTextures(1, &src_texnum);
	check_error();
	glBindTexture(GL_TEXTURE_2D, src_texnum);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	check_error();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	check_error();
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 512, 1, 0, GL_RED, GL_FLOAT, texdata);
	check_error();

	// Basic state.
	glDisable(GL_BLEND);
	check_error();
	glDisable(GL_DEPTH_TEST);
	check_error();
	glDepthMask(GL_FALSE);
	check_error();

	glViewport(0, 0, 512, 1);

	vector<string> frag_shader_outputs;
	GLuint glsl_program_num = resource_pool.compile_glsl_program(
		read_version_dependent_file("vs", "vert"),
		read_version_dependent_file("texture1d", "frag"),
		frag_shader_outputs);
	glUseProgram(glsl_program_num);
	check_error();
	glUniform1i(glGetUniformLocation(glsl_program_num, "tex"), 0);  // Bind the 2D sampler.

	// Draw the texture stretched over a long quad, interpolating it out.
	float vertices[] = {
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 1.0f,
		1.0f, 0.0f
	};

	GLuint vao;
	glGenVertexArrays(1, &vao);
	check_error();
	glBindVertexArray(vao);
	check_error();

	GLuint position_vbo = fill_vertex_attribute(glsl_program_num, "position", 2, GL_FLOAT, sizeof(vertices), vertices);
	GLuint texcoord_vbo = fill_vertex_attribute(glsl_program_num, "texcoord", 2, GL_FLOAT, sizeof(vertices), vertices);  // Same data.

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	check_error();

	cleanup_vertex_attribute(glsl_program_num, "position", position_vbo);
	cleanup_vertex_attribute(glsl_program_num, "texcoord", texcoord_vbo);

	glUseProgram(0);
	check_error();

	// Now read the data back and see what the card did. (Ignore the last value.)
	// (We only look at the red channel; the others will surely be the same.)
	unsigned char out_data[512 * 4];
	glReadPixels(0, 0, 512, 1, GL_RGBA, GL_UNSIGNED_BYTE, out_data);
	check_error();

	int wrongly_rounded = 0;
	for (unsigned i = 0; i < 255; ++i) {
		if (out_data[(i * 2 + 0) * 4] != i) {
			++wrongly_rounded;
		}
		if (out_data[(i * 2 + 1) * 4] != i + 1) {
			++wrongly_rounded;
		}
	}

	movit_num_wrongly_rounded = wrongly_rounded;

	// Clean up.
	glBindTexture(GL_TEXTURE_2D, 0);
	check_error();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	check_error();
	glDeleteFramebuffers(1, &fbo);
	check_error();
	glDeleteTextures(1, &dst_texnum);
	check_error();
	glDeleteTextures(1, &src_texnum);
	check_error();

	resource_pool.release_glsl_program(glsl_program_num);
	glDeleteVertexArrays(1, &vao);
	check_error();
}

bool check_extensions()
{
	// GLES generally doesn't use extensions as actively as desktop OpenGL.
	// For now, we say that for GLES, we require GLES 3, which has everything
	// we need.
	if (!epoxy_is_desktop_gl()) {
		if (epoxy_gl_version() >= 30) {
			return true;
		} else {
			fprintf(stderr, "Movit system requirements: GLES version %.1f is too old (GLES 3.0 needed).\n",
				0.1f * epoxy_gl_version());
			fprintf(stderr, "Movit initialization failed.\n");
			return false;
		}
	}

	if (epoxy_gl_version() < 30) {
		fprintf(stderr, "Movit system requirements: OpenGL version %.1f is too old (OpenGL 3.0 needed).\n",
			0.1f * epoxy_gl_version());
		fprintf(stderr, "Movit initialization failed.\n");
		return false;
	}

	// The user can specify that they want a timing report for each
	// phase in an effect chain. However, that depends on this extension;
	// without it, we do cannot even create the query objects.
	movit_timer_queries_supported =
		(epoxy_gl_version() >= 33 || epoxy_has_gl_extension("GL_ARB_timer_query"));

	// Certain effects have compute shader implementations, which may be
	// more efficient than the normal fragment shader versions.
	// GLSL 3.10 supposedly also has compute shaders, but I haven't tested them,
	// so we require desktop OpenGL.
	movit_compute_shaders_supported =
		(epoxy_is_desktop_gl() &&
		 (epoxy_gl_version() >= 43 ||
		  (epoxy_has_gl_extension("GL_ARB_compute_shader") &&
		   epoxy_has_gl_extension("GL_ARB_shader_image_load_store") &&
	           epoxy_has_gl_extension("GL_ARB_shader_image_size"))));

	return true;
}

double get_glsl_version()
{
	char *glsl_version_str = strdup((const char *)glGetString(GL_SHADING_LANGUAGE_VERSION));

	// Skip past the first period.
	char *ptr = strchr(glsl_version_str, '.');
	assert(ptr != nullptr);
	++ptr;

	// Now cut the string off at the next period or space, whatever comes first
	// (unless the string ends first).
	while (*ptr && *ptr != '.' && *ptr != ' ') {
		++ptr;
	}
	*ptr = '\0';

	// Now we have something on the form X.YY. We convert it to a float, and hope
	// that if it's inexact (e.g. 1.30), atof() will round the same way the
	// compiler will.
	std::istringstream locale_convert(glsl_version_str);
	locale_convert.imbue(std::locale("C"));
	double glsl_version;
	locale_convert >> glsl_version;
	free(glsl_version_str);

	return glsl_version;
}

void APIENTRY debug_callback(GLenum source,
                             GLenum type,
                             GLuint id,
                             GLenum severity,
                             GLsizei length,
                             const char *message,
                             const void *userParam)
#ifdef __GNUC__
	__attribute__((unused))
#endif
;

void APIENTRY debug_callback(GLenum source,
                             GLenum type,
                             GLuint id,
                             GLenum severity,
                             GLsizei length,
                             const char *message,
                             const void *userParam)
{
	printf("Debug: %s\n", message);
}

}  // namespace

bool init_movit(const string& data_directory, MovitDebugLevel debug_level)
{
	if (movit_initialized) {
		return true;
	}

	movit_data_directory = new string(data_directory);
	movit_debug_level = debug_level;

	// geez	
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glDisable(GL_DITHER);

	// You can turn this on if you want detailed debug messages from the driver.
	// You should probably also ask for a debug context (see gtest_sdl_main.cpp),
	// or you might not get much data back.
	// glDebugMessageCallbackARB(callback, nullptr);
	// glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, 0, GL_TRUE);

	if (!check_extensions()) {
		return false;
	}

	// Find out what shader model we should compile for.
	// We need at least 1.30, due to use of (among others) integers.
	if (epoxy_is_desktop_gl()) {
		if (get_glsl_version() < 1.30f) {
			fprintf(stderr, "Movit system requirements: Needs at least GLSL version 1.30 (has version %.1f)\n",
				get_glsl_version());
			return false;
		}
		if (get_glsl_version() < 1.50f) {
			movit_shader_model = MOVIT_GLSL_130;
		} else {
			// Note: All of our 1.50 shaders are identical to our 1.30 shaders,
			// but OS X does not support 1.30; only 1.10 (which we don't support
			// anymore) and 1.50 (and then only with core contexts). So we keep
			// a second set of shaders around whose only difference is the different
			// #version declaration.
			movit_shader_model = MOVIT_GLSL_150;
		}
	} else {
		movit_shader_model = MOVIT_ESSL_300;
	}

	measure_texel_subpixel_precision();
	measure_roundoff_problems();

	movit_initialized = true;
	return true;
}

}  // namespace movit
