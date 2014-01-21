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

#include <list>
#include <map>
#include <string>
#include <utility>
#include <GL/glew.h>
#include <pthread.h>

class ResourcePool {
public:
	// program_freelist_max_length is how many compiled programs that are unused to keep
	// around after they are no longer in use (in case another EffectChain
	// wants that exact program later). Shaders are expensive to compile and do not
	// need a lot of resources to keep around, so this should be a reasonable number.
	ResourcePool(size_t program_freelist_max_length = 100);
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
	//
	// Note: Currently we do not actually have a freelist, but this will change soon.
	GLuint create_2d_texture(GLint internal_format, GLsizei width, GLsizei height);
	void release_2d_texture(GLuint texture_num);

private:
	// Delete the given program and both its shaders.
	void delete_program(GLuint program_num);

	// Protects all the other elements in the class.
	pthread_mutex_t lock;

	size_t program_freelist_max_length;
		
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
};

#endif  // !defined(_MOVIT_RESOURCE_POOL_H)
