#ifndef _MOVIT_RESOURCE_POOL_H
#define _MOVIT_RESOURCE_POOL_H 1

// A ResourcePool governs resources that are shared between multiple EffectChains;
// in particular, resources that might be expensive to acquire or hold. Thus,
// if you have many EffectChains, hooking them up to the same ResourcePool is
// probably a good idea.
//
// However, hooking an EffectChain to a ResourcePool extends the OpenGL context
// demands (see effect_chain.h) to that of the ResourcePool; all chains must then
// only be used in OpenGL contexts sharing resources with each other. This is
// the reason why there isn't just one global ResourcePool singleton (although
// most practical users will just want one).
//
// Thread-safety: All functions except the constructor and destructor can be
// safely called from multiple threads at the same time, provided they have
// separate (but sharing) OpenGL contexts.

#include <GL/glew.h>
#include <pthread.h>
#include <stddef.h>
#include <list>
#include <map>
#include <string>
#include <utility>

namespace movit {

class ResourcePool {
public:
	// program_freelist_max_length is how many compiled programs that are unused to keep
	// around after they are no longer in use (in case another EffectChain
	// wants that exact program later). Shaders are expensive to compile and do not
	// need a lot of resources to keep around, so this should be a reasonable number.
	//
	// texture_freelist_max_bytes is how many bytes of unused textures to keep around
	// after they are no longer in use (in case a new texture of the same dimensions
	// and format is needed). Note that the size estimate is very coarse; it does not
	// take into account padding, metadata, and most importantly mipmapping.
	// This means you should be prepared for actual memory usage of the freelist being
	// twice this estimate or more.
	ResourcePool(size_t program_freelist_max_length = 100,
	             size_t texture_freelist_max_bytes = 100 << 20,  // 100 MB.
	             size_t fbo_freelist_max_length = 100);
	~ResourcePool();

	// All remaining functions are intended for calls from EffectChain only.

	// Compile the given vertex+fragment shader pair, or fetch an already
	// compiled program from the cache if possible. Keeps ownership of the
	// program; you must call release_glsl_program() instead of deleting it
	// when you no longer want it.
	GLuint compile_glsl_program(const std::string& vertex_shader, const std::string& fragment_shader);
	void release_glsl_program(GLuint glsl_program_num);

	// Allocate a 2D texture of the given internal format and dimensions,
	// or fetch a previous used if possible. Unbinds GL_TEXTURE_2D afterwards.
	// Keeps ownership of the texture; you must call release_2d_texture() instead
	// of deleting it when you no longer want it.
	GLuint create_2d_texture(GLint internal_format, GLsizei width, GLsizei height);
	void release_2d_texture(GLuint texture_num);

	// Allocate an FBO used for the given internal format and dimensions,
	// or fetch a previous used if possible. Keeps ownership of the FBO;
	// you must call release_fbo() of deleting it when you no longer want it.
	// You can get an appropriate context pointer from get_gl_context_identifier().
	//
	// NOTE: In principle, the FBO doesn't have a resolution or pixel format;
	// you can bind almost whatever texture you want to it. However, changing
	// resolution or pixel formats can have an adverse effect on performance,
	// in particular on NVidia cards. Also, keep in mind that FBOs are not
	// shareable across contexts.
	GLuint create_fbo(void *context, GLint internal_format, GLsizei width, GLsizei height);
	void release_fbo(GLuint fbo_num);

private:
	// Delete the given program and both its shaders.
	void delete_program(GLuint program_num);

	// Protects all the other elements in the class.
	pthread_mutex_t lock;

	size_t program_freelist_max_length, texture_freelist_max_bytes, fbo_freelist_max_length;
		
	// A mapping from vertex/fragment shader source strings to compiled program number.
	std::map<std::pair<std::string, std::string>, GLuint> programs;

	// A mapping from compiled program number to number of current users.
	// Once this reaches zero, the program is taken out of this map and instead
	// put on the freelist (after which it may be deleted).
	std::map<GLuint, int> program_refcount;

	// A mapping from program number to vertex and fragment shaders.
	std::map<GLuint, std::pair<GLuint, GLuint> > program_shaders;

	// A list of programs that are no longer in use, most recently freed first.
	// Once this reaches <program_freelist_max_length>, the last element
	// will be deleted.
	std::list<GLuint> program_freelist;

	struct Texture2D {
		GLint internal_format;
		GLsizei width, height;
	};

	// A mapping from texture number to format details. This is filled if the
	// texture is given out to a client or on the freelist, but not if it is
	// deleted from the freelist.
	std::map<GLuint, Texture2D> texture_formats;

	// A list of all textures that are release but not freed (most recently freed
	// first), and an estimate of their current memory usage. Once
	// <texture_freelist_bytes> goes above <texture_freelist_max_bytes>,
	// elements are deleted off the end of the list until we are under the limit
	// again.
	std::list<GLuint> texture_freelist;
	size_t texture_freelist_bytes;

	struct FBO {
		void *context;
		GLint internal_format;
		GLsizei width, height;
	};

	// A mapping from FBO number to format details. This is filled if the
	// FBO is given out to a client or on the freelist, but not if it is
	// deleted from the freelist.
	std::map<GLuint, FBO> fbo_formats;

	// A list of all FBOs that are release but not freed (most recently freed
	// first). Once this reaches <fbo_freelist_max_length>, the last element
	// will be deleted.
	std::list<GLuint> fbo_freelist;

	// See the caveats at the constructor.
	static size_t estimate_texture_size(const Texture2D &texture_format);
};

}  // namespace movit

#endif  // !defined(_MOVIT_RESOURCE_POOL_H)
