#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <epoxy/gl.h>

#include "init.h"
#include "resource_pool.h"
#include "util.h"

using namespace std;

namespace movit {

ResourcePool::ResourcePool(size_t program_freelist_max_length,
                           size_t texture_freelist_max_bytes,
                           size_t fbo_freelist_max_length)
	: program_freelist_max_length(program_freelist_max_length),
	  texture_freelist_max_bytes(texture_freelist_max_bytes),
	  fbo_freelist_max_length(fbo_freelist_max_length),
	  texture_freelist_bytes(0)
{
	pthread_mutex_init(&lock, NULL);
}

ResourcePool::~ResourcePool()
{
	assert(program_refcount.empty());

	for (list<GLuint>::const_iterator freelist_it = program_freelist.begin();
	     freelist_it != program_freelist.end();
	     ++freelist_it) {
		delete_program(*freelist_it);
	}
	assert(programs.empty());
	assert(program_shaders.empty());

	for (list<GLuint>::const_iterator freelist_it = texture_freelist.begin();
	     freelist_it != texture_freelist.end();
	     ++freelist_it) {
		GLuint free_texture_num = *freelist_it;
		assert(texture_formats.count(free_texture_num) != 0);
		texture_freelist_bytes -= estimate_texture_size(texture_formats[free_texture_num]);
		texture_formats.erase(free_texture_num);
		glDeleteTextures(1, &free_texture_num);
		check_error();
	}
	assert(texture_formats.empty());
	assert(texture_freelist_bytes == 0);

	void *context = get_gl_context_identifier();
	cleanup_unlinked_fbos(context);

	for (map<void *, std::list<GLuint> >::iterator context_it = fbo_freelist.begin();
	     context_it != fbo_freelist.end();
	     ++context_it) {
		if (context_it->first != context) {
			// If this does not hold, the client should have called clean_context() earlier.
			assert(context_it->second.empty());
			continue;
		}
		for (list<GLuint>::const_iterator freelist_it = context_it->second.begin();
		     freelist_it != context_it->second.end();
		     ++freelist_it) {
			pair<void *, GLuint> key(context, *freelist_it);
			GLuint free_fbo_num = *freelist_it;
			assert(fbo_formats.count(key) != 0);
			fbo_formats.erase(key);
			glDeleteFramebuffers(1, &free_fbo_num);
			check_error();
		}
	}

	assert(fbo_formats.empty());
}

void ResourcePool::delete_program(GLuint glsl_program_num)
{
	bool found_program = false;
	for (map<pair<string, string>, GLuint>::iterator program_it = programs.begin();
	     program_it != programs.end();
	     ++program_it) {
		if (program_it->second == glsl_program_num) {
			programs.erase(program_it);
			found_program = true;
			break;
		}
	}
	assert(found_program);
	glDeleteProgram(glsl_program_num);

	map<GLuint, pair<GLuint, GLuint> >::iterator shader_it =
		program_shaders.find(glsl_program_num);
	assert(shader_it != program_shaders.end());

	glDeleteShader(shader_it->second.first);
	glDeleteShader(shader_it->second.second);
	program_shaders.erase(shader_it);
}

GLuint ResourcePool::compile_glsl_program(const string& vertex_shader, const string& fragment_shader)
{
	GLuint glsl_program_num;
	pthread_mutex_lock(&lock);
	const pair<string, string> key(vertex_shader, fragment_shader);
	if (programs.count(key)) {
		// Already in the cache. Increment the refcount, or take it off the freelist
		// if it's zero.
		glsl_program_num = programs[key];
		map<GLuint, int>::iterator refcount_it = program_refcount.find(glsl_program_num);
		if (refcount_it != program_refcount.end()) {
			++refcount_it->second;
		} else {
			list<GLuint>::iterator freelist_it =
				find(program_freelist.begin(), program_freelist.end(), glsl_program_num);
			assert(freelist_it != program_freelist.end());
			program_freelist.erase(freelist_it);
			program_refcount.insert(make_pair(glsl_program_num, 1));
		}
	} else {
		// Not in the cache. Compile the shaders.
		glsl_program_num = glCreateProgram();
		check_error();
		GLuint vs_obj = compile_shader(vertex_shader, GL_VERTEX_SHADER);
		check_error();
		GLuint fs_obj = compile_shader(fragment_shader, GL_FRAGMENT_SHADER);
		check_error();
		glAttachShader(glsl_program_num, vs_obj);
		check_error();
		glAttachShader(glsl_program_num, fs_obj);
		check_error();
		glLinkProgram(glsl_program_num);
		check_error();

		GLint success;
		glGetProgramiv(glsl_program_num, GL_LINK_STATUS, &success);
		if (success == GL_FALSE) {
			GLchar error_log[1024] = {0};
			glGetProgramInfoLog(glsl_program_num, 1024, NULL, error_log);
			fprintf(stderr, "Error linking program: %s\n", error_log);
			exit(1);
		}

		if (movit_debug_level == MOVIT_DEBUG_ON) {
			// Output shader to a temporary file, for easier debugging.
			static int compiled_shader_num = 0;
			char filename[256];
			sprintf(filename, "chain-%03d.frag", compiled_shader_num++);
			FILE *fp = fopen(filename, "w");
			if (fp == NULL) {
				perror(filename);
				exit(1);
			}
			fprintf(fp, "%s\n", fragment_shader.c_str());
			fclose(fp);
		}

		programs.insert(make_pair(key, glsl_program_num));
		program_refcount.insert(make_pair(glsl_program_num, 1));
		program_shaders.insert(make_pair(glsl_program_num, make_pair(vs_obj, fs_obj)));
	}
	pthread_mutex_unlock(&lock);
	return glsl_program_num;
}

void ResourcePool::release_glsl_program(GLuint glsl_program_num)
{
	pthread_mutex_lock(&lock);
	map<GLuint, int>::iterator refcount_it = program_refcount.find(glsl_program_num);
	assert(refcount_it != program_refcount.end());

	if (--refcount_it->second == 0) {
		program_refcount.erase(refcount_it);
		assert(find(program_freelist.begin(), program_freelist.end(), glsl_program_num)
			== program_freelist.end());
		program_freelist.push_front(glsl_program_num);
		if (program_freelist.size() > program_freelist_max_length) {
			delete_program(program_freelist.back());
			program_freelist.pop_back();
		}
	}

	pthread_mutex_unlock(&lock);
}

GLuint ResourcePool::create_2d_texture(GLint internal_format, GLsizei width, GLsizei height)
{
	assert(width > 0);
	assert(height > 0);

	pthread_mutex_lock(&lock);
	// See if there's a texture on the freelist we can use.
	for (list<GLuint>::iterator freelist_it = texture_freelist.begin();
	     freelist_it != texture_freelist.end();
	     ++freelist_it) {
		GLuint texture_num = *freelist_it;
		map<GLuint, Texture2D>::const_iterator format_it = texture_formats.find(texture_num);
		assert(format_it != texture_formats.end());
		if (format_it->second.internal_format == internal_format &&
		    format_it->second.width == width &&
		    format_it->second.height == height) {
			texture_freelist_bytes -= estimate_texture_size(format_it->second);
			texture_freelist.erase(freelist_it);
			pthread_mutex_unlock(&lock);
			return texture_num;
		}
	}

	// Find any reasonable format given the internal format; OpenGL validates it
	// even though we give NULL as pointer.
	GLenum format;
	switch (internal_format) {
	case GL_RGBA32F_ARB:
	case GL_RGBA16F_ARB:
	case GL_RGBA8:
	case GL_SRGB8_ALPHA8:
		format = GL_RGBA;
		break;
	case GL_RGB32F:
	case GL_RGB16F:
	case GL_RGB8:
	case GL_SRGB8:
		format = GL_RGB;
		break;
	case GL_RG32F:
	case GL_RG16F:
	case GL_RG8:
		format = GL_RG;
		break;
	case GL_R32F:
	case GL_R16F:
	case GL_R8:
		format = GL_RED;
		break;
	default:
		// TODO: Add more here as needed.
		assert(false);
	}

	// Same with type; GLES is stricter than desktop OpenGL here.
	GLenum type;
	switch (internal_format) {
	case GL_RGBA32F_ARB:
	case GL_RGBA16F_ARB:
	case GL_RGB32F:
	case GL_RGB16F:
	case GL_RG32F:
	case GL_RG16F:
	case GL_R32F:
	case GL_R16F:
		type = GL_FLOAT;
		break;
	case GL_SRGB8_ALPHA8:
	case GL_SRGB8:
	case GL_RGBA8:
	case GL_RGB8:
	case GL_RG8:
	case GL_R8:
		type = GL_UNSIGNED_BYTE;
		break;
	default:
		// TODO: Add more here as needed.
		assert(false);
	}


	GLuint texture_num;
	glGenTextures(1, &texture_num);
	check_error();
	glBindTexture(GL_TEXTURE_2D, texture_num);
	check_error();
	glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, type, NULL);
	check_error();
	glBindTexture(GL_TEXTURE_2D, 0);
	check_error();

	Texture2D texture_format;
	texture_format.internal_format = internal_format;
	texture_format.width = width;
	texture_format.height = height;
	assert(texture_formats.count(texture_num) == 0);
	texture_formats.insert(make_pair(texture_num, texture_format));

	pthread_mutex_unlock(&lock);
	return texture_num;
}

void ResourcePool::release_2d_texture(GLuint texture_num)
{
	pthread_mutex_lock(&lock);
	texture_freelist.push_front(texture_num);
	assert(texture_formats.count(texture_num) != 0);
	texture_freelist_bytes += estimate_texture_size(texture_formats[texture_num]);

	while (texture_freelist_bytes > texture_freelist_max_bytes) {
		GLuint free_texture_num = texture_freelist.back();
		texture_freelist.pop_back();
		assert(texture_formats.count(free_texture_num) != 0);
		texture_freelist_bytes -= estimate_texture_size(texture_formats[free_texture_num]);
		texture_formats.erase(free_texture_num);
		glDeleteTextures(1, &free_texture_num);
		check_error();

		// Unlink any lingering FBO related to this texture. We might
		// not be in the right context, so don't delete it right away;
		// the cleanup in release_fbo() (which calls cleanup_unlinked_fbos())
		// will take care of actually doing that later.
		for (map<pair<void *, GLuint>, FBO>::iterator format_it = fbo_formats.begin();
		     format_it != fbo_formats.end();
		     ++format_it) {
			for (unsigned i = 0; i < num_fbo_attachments; ++i) {
				if (format_it->second.texture_num[i] == free_texture_num) {
					format_it->second.texture_num[i] = GL_INVALID_INDEX;
				}
			}
		}
	}
	pthread_mutex_unlock(&lock);
}

GLuint ResourcePool::create_fbo(GLuint texture0_num, GLuint texture1_num, GLuint texture2_num, GLuint texture3_num)
{
	void *context = get_gl_context_identifier();

	// Make sure we are filled from the bottom.
	assert(texture0_num != 0);
	if (texture1_num == 0) {
		assert(texture2_num == 0);
	}
	if (texture2_num == 0) {
		assert(texture3_num == 0);
	}

	pthread_mutex_lock(&lock);
	if (fbo_freelist.count(context) != 0) {
		// See if there's an FBO on the freelist we can use.
		for (list<GLuint>::iterator freelist_it = fbo_freelist[context].begin();
		     freelist_it != fbo_freelist[context].end();
		     ++freelist_it) {
			GLuint fbo_num = *freelist_it;
			map<pair<void *, GLuint>, FBO>::const_iterator format_it =
				fbo_formats.find(make_pair(context, fbo_num));
			assert(format_it != fbo_formats.end());
			if (format_it->second.texture_num[0] == texture0_num &&
			    format_it->second.texture_num[1] == texture1_num &&
			    format_it->second.texture_num[2] == texture2_num &&
			    format_it->second.texture_num[3] == texture3_num) {
				fbo_freelist[context].erase(freelist_it);
				pthread_mutex_unlock(&lock);
				return fbo_num;
			}
		}
	}

	// Create a new one.
	FBO fbo_format;
	fbo_format.texture_num[0] = texture0_num;
	fbo_format.texture_num[1] = texture1_num;
	fbo_format.texture_num[2] = texture2_num;
	fbo_format.texture_num[3] = texture3_num;

	GLuint fbo_num;
	glGenFramebuffers(1, &fbo_num);
	check_error();
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_num);
	check_error();

	GLenum bufs[num_fbo_attachments];
	unsigned num_active_attachments = 0;
	for (unsigned i = 0; i < num_fbo_attachments; ++i, ++num_active_attachments) {
		if (fbo_format.texture_num[i] == 0) {
			break;
		}
		glFramebufferTexture2D(
			GL_FRAMEBUFFER,
			GL_COLOR_ATTACHMENT0 + i,
			GL_TEXTURE_2D,
			fbo_format.texture_num[i],
			0);
		check_error();
		bufs[i] = GL_COLOR_ATTACHMENT0 + i;
	}

	glDrawBuffers(num_active_attachments, bufs);
	check_error();

	GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
	assert(status == GL_FRAMEBUFFER_COMPLETE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	check_error();

	pair<void *, GLuint> key(context, fbo_num);
	assert(fbo_formats.count(key) == 0);
	fbo_formats.insert(make_pair(key, fbo_format));

	pthread_mutex_unlock(&lock);
	return fbo_num;
}

void ResourcePool::release_fbo(GLuint fbo_num)
{
	void *context = get_gl_context_identifier();

	pthread_mutex_lock(&lock);
	fbo_freelist[context].push_front(fbo_num);
	assert(fbo_formats.count(make_pair(context, fbo_num)) != 0);

	// Now that we're in this context, free up any FBOs that are connected
	// to deleted textures (in release_2d_texture).
	cleanup_unlinked_fbos(context);

	shrink_fbo_freelist(context, fbo_freelist_max_length);
	pthread_mutex_unlock(&lock);
}

void ResourcePool::clean_context()
{
	void *context = get_gl_context_identifier();

	// Currently, we only need to worry about FBOs, as they are the only
	// non-shareable resource we hold.
	shrink_fbo_freelist(context, 0);
	fbo_freelist.erase(context);
}

void ResourcePool::cleanup_unlinked_fbos(void *context)
{
	for (list<GLuint>::iterator freelist_it = fbo_freelist[context].begin();
	     freelist_it != fbo_freelist[context].end(); ) {
		GLuint fbo_num = *freelist_it;
		pair<void *, GLuint> key(context, fbo_num);
		assert(fbo_formats.count(key) != 0);

		bool all_unlinked = true;
		for (unsigned i = 0; i < num_fbo_attachments; ++i) {
			if (fbo_formats[key].texture_num[i] != 0 &&
			    fbo_formats[key].texture_num[i] != GL_INVALID_INDEX) {
				all_unlinked = false;
				break;
			}
		}
		if (all_unlinked) {
			fbo_formats.erase(key);
			glDeleteFramebuffers(1, &fbo_num);
			check_error();
			fbo_freelist[context].erase(freelist_it++);
		} else {
			freelist_it++;
		}
	}
}

void ResourcePool::shrink_fbo_freelist(void *context, size_t max_length)
{
	while (fbo_freelist[context].size() > max_length) {
		GLuint free_fbo_num = fbo_freelist[context].back();
		pair<void *, GLuint> key(context, free_fbo_num);
		fbo_freelist[context].pop_back();
		assert(fbo_formats.count(key) != 0);
		fbo_formats.erase(key);
		glDeleteFramebuffers(1, &free_fbo_num);
		check_error();
	}
}

size_t ResourcePool::estimate_texture_size(const Texture2D &texture_format)
{
	size_t bytes_per_pixel;

	switch (texture_format.internal_format) {
	case GL_RGBA32F_ARB:
		bytes_per_pixel = 16;
		break;
	case GL_RGBA16F_ARB:
		bytes_per_pixel = 8;
		break;
	case GL_RGB32F_ARB:
		bytes_per_pixel = 12;
		break;
	case GL_RGB16F_ARB:
		bytes_per_pixel = 6;
		break;
	case GL_RGBA8:
	case GL_SRGB8_ALPHA8:
		bytes_per_pixel = 4;
		break;
	case GL_RGB8:
	case GL_SRGB8:
		bytes_per_pixel = 3;
		break;
	case GL_RG32F:
		bytes_per_pixel = 8;
		break;
	case GL_RG16F:
		bytes_per_pixel = 4;
		break;
	case GL_R32F:
		bytes_per_pixel = 4;
		break;
	case GL_R16F:
		bytes_per_pixel = 2;
		break;
	case GL_RG8:
		bytes_per_pixel = 2;
		break;
	case GL_R8:
		bytes_per_pixel = 1;
		break;
	default:
		// TODO: Add more here as needed.
		assert(false);
	}

	return texture_format.width * texture_format.height * bytes_per_pixel;
}

}  // namespace movit
