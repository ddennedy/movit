#include "resource_pool.h"

#include <stdio.h>
#include <pthread.h>

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include "init.h"
#include "util.h"

using namespace std;

ResourcePool::ResourcePool(size_t program_freelist_max_length)
	: program_freelist_max_length(program_freelist_max_length) {
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
}

void ResourcePool::delete_program(GLuint glsl_program_num)
{
	bool found_program = false;
	for (std::map<std::pair<std::string, std::string>, GLuint>::iterator program_it = programs.begin();
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

	std::map<GLuint, std::pair<GLuint, GLuint> >::iterator shader_it =
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
		GLuint vs_obj = compile_shader(vertex_shader, GL_VERTEX_SHADER);
		GLuint fs_obj = compile_shader(fragment_shader, GL_FRAGMENT_SHADER);
		glAttachShader(glsl_program_num, vs_obj);
		check_error();
		glAttachShader(glsl_program_num, fs_obj);
		check_error();
		glLinkProgram(glsl_program_num);
		check_error();

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
	case GL_RG32F:
	case GL_RG16F:
		format = GL_RG;
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
	glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, GL_UNSIGNED_BYTE, NULL);
	check_error();
	glBindTexture(GL_TEXTURE_2D, 0);
	check_error();

	return texture_num;
}

void ResourcePool::release_2d_texture(GLuint texture_num)
{
	glDeleteTextures(1, &texture_num);
	check_error();
}
