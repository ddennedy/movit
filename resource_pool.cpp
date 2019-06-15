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
                           size_t fbo_freelist_max_length,
                           size_t vao_freelist_max_length)
	: program_freelist_max_length(program_freelist_max_length),
	  texture_freelist_max_bytes(texture_freelist_max_bytes),
	  fbo_freelist_max_length(fbo_freelist_max_length),
	  vao_freelist_max_length(vao_freelist_max_length),
	  texture_freelist_bytes(0)
{
	pthread_mutex_init(&lock, nullptr);
}

ResourcePool::~ResourcePool()
{
	assert(program_refcount.empty());

	for (GLuint program : program_freelist) {
		delete_program(program);
	}
	assert(programs.empty());
	assert(program_shaders.empty());

	for (GLuint free_texture_num : texture_freelist) {
		assert(texture_formats.count(free_texture_num) != 0);
		texture_freelist_bytes -= estimate_texture_size(texture_formats[free_texture_num]);
		glDeleteSync(texture_formats[free_texture_num].no_reuse_before);
		texture_formats.erase(free_texture_num);
		glDeleteTextures(1, &free_texture_num);
		check_error();
	}
	assert(texture_formats.empty());
	assert(texture_freelist_bytes == 0);

	void *context = get_gl_context_identifier();
	cleanup_unlinked_fbos(context);

	for (const auto &context_and_fbos : fbo_freelist) {
		if (context_and_fbos.first != context) {
			// If this does not hold, the client should have called clean_context() earlier.
			assert(context_and_fbos.second.empty());
			continue;
		}
		for (FBOFormatIterator fbo_it : context_and_fbos.second) {
			glDeleteFramebuffers(1, &fbo_it->second.fbo_num);
			check_error();
			fbo_formats.erase(fbo_it);
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
	for (map<string, GLuint>::iterator program_it = compute_programs.begin();
	     program_it != compute_programs.end();
	     ++program_it) {
		if (program_it->second == glsl_program_num) {
			compute_programs.erase(program_it);
			found_program = true;
			break;
		}
	}
	assert(found_program);

	map<GLuint, stack<GLuint>>::iterator instance_list_it = program_instances.find(glsl_program_num);
	assert(instance_list_it != program_instances.end());

	while (!instance_list_it->second.empty()) {
		GLuint instance_program_num = instance_list_it->second.top();
		instance_list_it->second.pop();
		glDeleteProgram(instance_program_num);
		program_masters.erase(instance_program_num);
	}
	program_instances.erase(instance_list_it);

	map<GLuint, ShaderSpec>::iterator shader_it =
		program_shaders.find(glsl_program_num);
	if (shader_it == program_shaders.end()) {
		// Should be a compute shader.
		map<GLuint, ComputeShaderSpec>::iterator compute_shader_it =
			compute_program_shaders.find(glsl_program_num);
		assert(compute_shader_it != compute_program_shaders.end());

		glDeleteShader(compute_shader_it->second.cs_obj);
		compute_program_shaders.erase(compute_shader_it);
	} else {
		glDeleteShader(shader_it->second.vs_obj);
		glDeleteShader(shader_it->second.fs_obj);
		program_shaders.erase(shader_it);
	}
}

GLuint ResourcePool::compile_glsl_program(const string& vertex_shader,
                                          const string& fragment_shader,
                                          const vector<string>& fragment_shader_outputs)
{
	GLuint glsl_program_num;
	pthread_mutex_lock(&lock);

	// Augment the fragment shader program text with the outputs, so that they become
	// part of the key. Also potentially useful for debugging.
	string fragment_shader_processed = fragment_shader;
	for (unsigned output_index = 0; output_index < fragment_shader_outputs.size(); ++output_index) {
		char buf[256];
		snprintf(buf, sizeof(buf), "// Bound output: %s\n", fragment_shader_outputs[output_index].c_str());
		fragment_shader_processed += buf;
	}

	const pair<string, string> key(vertex_shader, fragment_shader_processed);
	if (programs.count(key)) {
		// Already in the cache.
		glsl_program_num = programs[key];
		increment_program_refcount(glsl_program_num);
	} else {
		// Not in the cache. Compile the shaders.
		GLuint vs_obj = compile_shader(vertex_shader, GL_VERTEX_SHADER);
		check_error();
		GLuint fs_obj = compile_shader(fragment_shader_processed, GL_FRAGMENT_SHADER);
		check_error();
		glsl_program_num = link_program(vs_obj, fs_obj, fragment_shader_outputs);

		output_debug_shader(fragment_shader_processed, "frag");

		programs.insert(make_pair(key, glsl_program_num));
		add_master_program(glsl_program_num);

		ShaderSpec spec;
		spec.vs_obj = vs_obj;
		spec.fs_obj = fs_obj;
		spec.fragment_shader_outputs = fragment_shader_outputs;
		program_shaders.insert(make_pair(glsl_program_num, spec));
	}
	pthread_mutex_unlock(&lock);
	return glsl_program_num;
}

GLuint ResourcePool::link_program(GLuint vs_obj,
                                  GLuint fs_obj,
                                  const vector<string>& fragment_shader_outputs)
{
	GLuint glsl_program_num = glCreateProgram();
	check_error();
	glAttachShader(glsl_program_num, vs_obj);
	check_error();
	glAttachShader(glsl_program_num, fs_obj);
	check_error();

	// Bind the outputs, if we have multiple ones.
	if (fragment_shader_outputs.size() > 1) {
		for (unsigned output_index = 0; output_index < fragment_shader_outputs.size(); ++output_index) {
			glBindFragDataLocation(glsl_program_num, output_index,
					       fragment_shader_outputs[output_index].c_str());
		}
	}

	glLinkProgram(glsl_program_num);
	check_error();

	GLint success;
	glGetProgramiv(glsl_program_num, GL_LINK_STATUS, &success);
	if (success == GL_FALSE) {
		GLchar error_log[1024] = {0};
		glGetProgramInfoLog(glsl_program_num, 1024, nullptr, error_log);
		fprintf(stderr, "Error linking program: %s\n", error_log);
		exit(1);
	}

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

GLuint ResourcePool::compile_glsl_compute_program(const string& compute_shader)
{
	GLuint glsl_program_num;
	pthread_mutex_lock(&lock);

	const string &key = compute_shader;
	if (compute_programs.count(key)) {
		// Already in the cache.
		glsl_program_num = compute_programs[key];
		increment_program_refcount(glsl_program_num);
	} else {
		// Not in the cache. Compile the shader.
		GLuint cs_obj = compile_shader(compute_shader, GL_COMPUTE_SHADER);
		check_error();
		glsl_program_num = link_compute_program(cs_obj);

		output_debug_shader(compute_shader, "comp");

		compute_programs.insert(make_pair(key, glsl_program_num));
		add_master_program(glsl_program_num);

		ComputeShaderSpec spec;
		spec.cs_obj = cs_obj;
		compute_program_shaders.insert(make_pair(glsl_program_num, spec));
	}
	pthread_mutex_unlock(&lock);
	return glsl_program_num;
}

GLuint ResourcePool::link_compute_program(GLuint cs_obj)
{
	GLuint glsl_program_num = glCreateProgram();
	check_error();
	glAttachShader(glsl_program_num, cs_obj);
	check_error();
	glLinkProgram(glsl_program_num);
	check_error();

	GLint success;
	glGetProgramiv(glsl_program_num, GL_LINK_STATUS, &success);
	if (success == GL_FALSE) {
		GLchar error_log[1024] = {0};
		glGetProgramInfoLog(glsl_program_num, 1024, nullptr, error_log);
		fprintf(stderr, "Error linking program: %s\n", error_log);
		exit(1);
	}

	return glsl_program_num;
}

GLuint ResourcePool::use_glsl_program(GLuint glsl_program_num)
{
	pthread_mutex_lock(&lock);
	assert(program_instances.count(glsl_program_num));
	stack<GLuint> &instances = program_instances[glsl_program_num];

	GLuint instance_program_num;
	if (!instances.empty()) {
		// There's an unused instance of this program; just return it.
		instance_program_num = instances.top();
		instances.pop();
	} else {
		// We need to clone this program. (unuse_glsl_program()
		// will later put it onto the list.)
		map<GLuint, ShaderSpec>::iterator shader_it =
			program_shaders.find(glsl_program_num);
		if (shader_it == program_shaders.end()) {
			// Should be a compute shader.
			map<GLuint, ComputeShaderSpec>::iterator compute_shader_it =
				compute_program_shaders.find(glsl_program_num);
			instance_program_num = link_compute_program(
				compute_shader_it->second.cs_obj);
		} else {
			// A regular fragment shader.
			instance_program_num = link_program(
				shader_it->second.vs_obj,
				shader_it->second.fs_obj,
				shader_it->second.fragment_shader_outputs);
		}
		program_masters.insert(make_pair(instance_program_num, glsl_program_num));
	}
	pthread_mutex_unlock(&lock);

	glUseProgram(instance_program_num);
	return instance_program_num;
}

void ResourcePool::unuse_glsl_program(GLuint instance_program_num)
{
	pthread_mutex_lock(&lock);

	auto master_it = program_masters.find(instance_program_num);
	assert(master_it != program_masters.end());

	assert(program_instances.count(master_it->second));
	stack<GLuint> &instances = program_instances[master_it->second];

	instances.push(instance_program_num);

	pthread_mutex_unlock(&lock);
}

GLuint ResourcePool::create_2d_texture(GLint internal_format, GLsizei width, GLsizei height)
{
	assert(width > 0);
	assert(height > 0);

	pthread_mutex_lock(&lock);
	// See if there's a texture on the freelist we can use.
	for (auto freelist_it = texture_freelist.begin();
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
			GLsync sync = format_it->second.no_reuse_before;
			pthread_mutex_unlock(&lock);
			glWaitSync(sync, 0, GL_TIMEOUT_IGNORED);
			glDeleteSync(sync);
			return texture_num;
		}
	}

	// Find any reasonable format given the internal format; OpenGL validates it
	// even though we give nullptr as pointer.
	GLenum format;
	switch (internal_format) {
	case GL_RGBA32F_ARB:
	case GL_RGBA16F_ARB:
	case GL_RGBA16:
	case GL_RGBA8:
	case GL_RGB10_A2:
	case GL_SRGB8_ALPHA8:
		format = GL_RGBA;
		break;
	case GL_RGB32F:
	case GL_RGB16F:
	case GL_RGB16:
	case GL_R11F_G11F_B10F:
	case GL_RGB8:
	case GL_RGB10:
	case GL_SRGB8:
	case GL_RGB565:
	case GL_RGB9_E5:
		format = GL_RGB;
		break;
	case GL_RG32F:
	case GL_RG16F:
	case GL_RG16:
	case GL_RG8:
		format = GL_RG;
		break;
	case GL_R32F:
	case GL_R16F:
	case GL_R16:
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
	case GL_R11F_G11F_B10F:
	case GL_RGB9_E5:
	case GL_RG32F:
	case GL_RG16F:
	case GL_R32F:
	case GL_R16F:
		type = GL_FLOAT;
		break;
	case GL_RGBA16:
	case GL_RGB16:
	case GL_RG16:
	case GL_R16:
		type = GL_UNSIGNED_SHORT;
		break;
	case GL_SRGB8_ALPHA8:
	case GL_SRGB8:
	case GL_RGBA8:
	case GL_RGB8:
	case GL_RGB10_A2:
	case GL_RGB10:
	case GL_RG8:
	case GL_R8:
		type = GL_UNSIGNED_BYTE;
		break;
	case GL_RGB565:
		type = GL_UNSIGNED_SHORT_5_6_5;
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
	glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, type, nullptr);
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
	texture_formats[texture_num].no_reuse_before = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

	while (texture_freelist_bytes > texture_freelist_max_bytes) {
		GLuint free_texture_num = texture_freelist.back();
		texture_freelist.pop_back();
		assert(texture_formats.count(free_texture_num) != 0);
		texture_freelist_bytes -= estimate_texture_size(texture_formats[free_texture_num]);
		glDeleteSync(texture_formats[free_texture_num].no_reuse_before);
		texture_formats.erase(free_texture_num);
		glDeleteTextures(1, &free_texture_num);
		check_error();

		// Unlink any lingering FBO related to this texture. We might
		// not be in the right context, so don't delete it right away;
		// the cleanup in release_fbo() (which calls cleanup_unlinked_fbos())
		// will take care of actually doing that later.
		for (auto &key_and_fbo : fbo_formats) {
			for (unsigned i = 0; i < num_fbo_attachments; ++i) {
				if (key_and_fbo.second.texture_num[i] == free_texture_num) {
					key_and_fbo.second.texture_num[i] = GL_INVALID_INDEX;
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
		auto end = fbo_freelist[context].end();
		for (auto freelist_it = fbo_freelist[context].begin(); freelist_it != end; ++freelist_it) {
			FBOFormatIterator fbo_it = *freelist_it;
			if (fbo_it->second.texture_num[0] == texture0_num &&
			    fbo_it->second.texture_num[1] == texture1_num &&
			    fbo_it->second.texture_num[2] == texture2_num &&
			    fbo_it->second.texture_num[3] == texture3_num) {
				fbo_freelist[context].erase(freelist_it);
				pthread_mutex_unlock(&lock);
				return fbo_it->second.fbo_num;
			}
		}
	}

	// Create a new one.
	FBO fbo_format;
	fbo_format.texture_num[0] = texture0_num;
	fbo_format.texture_num[1] = texture1_num;
	fbo_format.texture_num[2] = texture2_num;
	fbo_format.texture_num[3] = texture3_num;

	glGenFramebuffers(1, &fbo_format.fbo_num);
	check_error();
	glBindFramebuffer(GL_FRAMEBUFFER, fbo_format.fbo_num);
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

	pair<void *, GLuint> key(context, fbo_format.fbo_num);
	assert(fbo_formats.count(key) == 0);
	fbo_formats.insert(make_pair(key, fbo_format));

	pthread_mutex_unlock(&lock);
	return fbo_format.fbo_num;
}

void ResourcePool::release_fbo(GLuint fbo_num)
{
	void *context = get_gl_context_identifier();

	pthread_mutex_lock(&lock);
	FBOFormatIterator fbo_it = fbo_formats.find(make_pair(context, fbo_num));
	assert(fbo_it != fbo_formats.end());
	fbo_freelist[context].push_front(fbo_it);

	// Now that we're in this context, free up any FBOs that are connected
	// to deleted textures (in release_2d_texture).
	cleanup_unlinked_fbos(context);

	shrink_fbo_freelist(context, fbo_freelist_max_length);
	pthread_mutex_unlock(&lock);
}

GLuint ResourcePool::create_vec2_vao(const set<GLint> &attribute_indices, GLuint vbo_num)
{
	void *context = get_gl_context_identifier();

	pthread_mutex_lock(&lock);
	if (vao_freelist.count(context) != 0) {
		// See if there's a VAO the freelist we can use.
		auto end = vao_freelist[context].end();
		for (auto freelist_it = vao_freelist[context].begin(); freelist_it != end; ++freelist_it) {
			VAOFormatIterator vao_it = *freelist_it;
			if (vao_it->second.vbo_num == vbo_num &&
			    vao_it->second.attribute_indices == attribute_indices) {
				vao_freelist[context].erase(freelist_it);
				pthread_mutex_unlock(&lock);
				return vao_it->second.vao_num;
			}
		}
	}

	// Create a new one.
	VAO vao_format;
	vao_format.attribute_indices = attribute_indices;
	vao_format.vbo_num = vbo_num;

	glGenVertexArrays(1, &vao_format.vao_num);
	check_error();
	glBindVertexArray(vao_format.vao_num);
	check_error();
	glBindBuffer(GL_ARRAY_BUFFER, vbo_num);
	check_error();

	for (GLint attr : attribute_indices) {
		glEnableVertexAttribArray(attr);
		check_error();
		glVertexAttribPointer(attr, 2, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
		check_error();
	}

	glBindVertexArray(0);
	check_error();
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	check_error();

	pair<void *, GLuint> key(context, vao_format.vao_num);
	assert(vao_formats.count(key) == 0);
	vao_formats.insert(make_pair(key, vao_format));

	pthread_mutex_unlock(&lock);
	return vao_format.vao_num;
}

void ResourcePool::release_vec2_vao(GLuint vao_num)
{
	void *context = get_gl_context_identifier();

	pthread_mutex_lock(&lock);
	VAOFormatIterator vao_it = vao_formats.find(make_pair(context, vao_num));
	assert(vao_it != vao_formats.end());
	vao_freelist[context].push_front(vao_it);

	shrink_vao_freelist(context, vao_freelist_max_length);
	pthread_mutex_unlock(&lock);
}

void ResourcePool::clean_context()
{
	void *context = get_gl_context_identifier();

	// Currently, we only need to worry about FBOs and VAOs, as they are the only
	// non-shareable resources we hold.
	shrink_fbo_freelist(context, 0);
	fbo_freelist.erase(context);

	shrink_vao_freelist(context, 0);
	vao_freelist.erase(context);
}

void ResourcePool::cleanup_unlinked_fbos(void *context)
{
	auto end = fbo_freelist[context].end();
	for (auto freelist_it = fbo_freelist[context].begin(); freelist_it != end; ) {
		FBOFormatIterator fbo_it = *freelist_it;

		bool all_unlinked = true;
		for (unsigned i = 0; i < num_fbo_attachments; ++i) {
			if (fbo_it->second.texture_num[i] != 0 &&
			    fbo_it->second.texture_num[i] != GL_INVALID_INDEX) {
				all_unlinked = false;
				break;
			}
		}
		if (all_unlinked) {
			glDeleteFramebuffers(1, &fbo_it->second.fbo_num);
			check_error();
			fbo_formats.erase(fbo_it);
			fbo_freelist[context].erase(freelist_it++);
		} else {
			freelist_it++;
		}
	}
}

void ResourcePool::shrink_fbo_freelist(void *context, size_t max_length)
{
	list<FBOFormatIterator> &freelist = fbo_freelist[context];
	while (freelist.size() > max_length) {
		FBOFormatIterator free_fbo_it = freelist.back();
		glDeleteFramebuffers(1, &free_fbo_it->second.fbo_num);
		check_error();
		fbo_formats.erase(free_fbo_it);
		freelist.pop_back();
	}
}

void ResourcePool::shrink_vao_freelist(void *context, size_t max_length)
{
	list<VAOFormatIterator> &freelist = vao_freelist[context];
	while (freelist.size() > max_length) {
		VAOFormatIterator free_vao_it = freelist.back();
		glDeleteVertexArrays(1, &free_vao_it->second.vao_num);
		check_error();
		vao_formats.erase(free_vao_it);
		freelist.pop_back();
	}
}

void ResourcePool::increment_program_refcount(GLuint program_num)
{
	map<GLuint, int>::iterator refcount_it = program_refcount.find(program_num);
	if (refcount_it != program_refcount.end()) {
		++refcount_it->second;
	} else {
		list<GLuint>::iterator freelist_it =
			find(program_freelist.begin(), program_freelist.end(), program_num);
		assert(freelist_it != program_freelist.end());
		program_freelist.erase(freelist_it);
		program_refcount.insert(make_pair(program_num, 1));
	}
}

void ResourcePool::output_debug_shader(const string &shader_src, const string &suffix)
{
	if (movit_debug_level == MOVIT_DEBUG_ON) {
		// Output shader to a temporary file, for easier debugging.
		static int compiled_shader_num = 0;
		char filename[256];
		sprintf(filename, "chain-%03d.%s", compiled_shader_num++, suffix.c_str());
		FILE *fp = fopen(filename, "w");
		if (fp == nullptr) {
			perror(filename);
			exit(1);
		}
		fprintf(fp, "%s\n", shader_src.c_str());
		fclose(fp);
	}
}

void ResourcePool::add_master_program(GLuint program_num)
{
	program_refcount.insert(make_pair(program_num, 1));
	stack<GLuint> instances;
	instances.push(program_num);
	program_instances.insert(make_pair(program_num, instances));
	program_masters.insert(make_pair(program_num, program_num));
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
	case GL_R11F_G11F_B10F:
		bytes_per_pixel = 4;
		break;
	case GL_RGB9_E5:
		bytes_per_pixel = 4;
		break;
	case GL_RGBA8:
	case GL_SRGB8_ALPHA8:
	case GL_RGB10_A2:
	case GL_RGB10:
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
	case GL_RGB565:
		bytes_per_pixel = 2;
		break;
	case GL_RGBA16:
		bytes_per_pixel = 8;
		break;
	case GL_RGB16:
		bytes_per_pixel = 6;
		break;
	case GL_RG16:
		bytes_per_pixel = 4;
		break;
	case GL_R16:
		bytes_per_pixel = 2;
		break;
	default:
		// TODO: Add more here as needed.
		assert(false);
	}

	return texture_format.width * texture_format.height * bytes_per_pixel;
}

}  // namespace movit
