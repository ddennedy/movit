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
//
// Memory management (only relevant if you use multiple contexts): Some objects,
// like FBOs, are not shareable across contexts, and can only be deleted from
// the context they were created in. Thus, you will need to tell the
// ResourcePool explicitly if you delete a context, or they will leak (and the
// ResourcePool destructor will assert-fail). See clean_context().

#include <epoxy/gl.h>
#include <pthread.h>
#include <stddef.h>
#include <list>
#include <map>
#include <set>
#include <stack>
#include <string>
#include <utility>
#include <vector>

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
	             size_t fbo_freelist_max_length = 100,  // Per context.
	             size_t vao_freelist_max_length = 100);  // Per context.
	~ResourcePool();

	// All remaining functions are intended for calls from EffectChain only.

	// Compile the given vertex+fragment shader pair, or fetch an already
	// compiled program from the cache if possible. Keeps ownership of the
	// program; you must call release_glsl_program() instead of deleting it
	// when you no longer want it.
	//
	// If <fragment_shader_outputs> contains more than one value, the given
	// outputs will be bound to fragment shader output colors in the order
	// they appear in the vector. Otherwise, output order is undefined and
	// determined by the OpenGL driver.
	GLuint compile_glsl_program(const std::string& vertex_shader,
	                            const std::string& fragment_shader,
	                            const std::vector<std::string>& frag_shader_outputs);
	void release_glsl_program(GLuint glsl_program_num);

	// Same as the previous, but for compile shaders instead. There is currently
	// no support for binding multiple outputs.
	GLuint compile_glsl_compute_program(const std::string& compile_shader);
	void release_glsl_compute_program(GLuint glsl_program_num);

	// Since uniforms belong to the program and not to the context,
	// a given GLSL program number can't be used by more than one thread
	// at a time. Thus, if two threads want to use the same program
	// (usually because two EffectChains share them via caching),
	// we will need to make a clone. use_glsl_program() makes such
	// a clone if needed, calls glUseProgram(), and returns the real
	// program number that was used; this must be given to
	// unuse_glsl_program() to release it. unuse_glsl_program() does not
	// actually change any OpenGL state, though.
	GLuint use_glsl_program(GLuint glsl_program_num);
	void unuse_glsl_program(GLuint instance_program_num);

	// Allocate a 2D texture of the given internal format and dimensions,
	// or fetch a previous used if possible. Unbinds GL_TEXTURE_2D afterwards.
	// Keeps ownership of the texture; you must call release_2d_texture() instead
	// of deleting it when you no longer want it.
	GLuint create_2d_texture(GLint internal_format, GLsizei width, GLsizei height);
	void release_2d_texture(GLuint texture_num);

	// Allocate an FBO with the the given texture(s) bound as framebuffer attachment(s),
	// or fetch a previous used if possible. Unbinds GL_FRAMEBUFFER afterwards.
	// Keeps ownership of the FBO; you must call release_fbo() of deleting
	// it when you no longer want it.
	//
	// NOTE: In principle, the FBO doesn't have a resolution or pixel format;
	// you can bind almost whatever texture you want to it. However, changing
	// textures can have an adverse effect on performance due to validation,
	// in particular on NVidia cards. Also, keep in mind that FBOs are not
	// shareable across contexts, so you must have the context that's supposed
	// to own the FBO current when you create or release it.
	GLuint create_fbo(GLuint texture0_num,
	                  GLuint texture1_num = 0,
	                  GLuint texture2_num = 0,
	                  GLuint texture3_num = 0);
	void release_fbo(GLuint fbo_num);

	// Create a VAO of a very specific form: All the given attribute indices
	// are bound to start of the  given VBO and contain two-component floats.
	// Keeps ownership of the VAO; you must call release_vec2_vao() of deleting
	// it when you no longer want it. VAOs are not sharable across contexts.
	//
	// These are not cached primarily for performance, but rather to work
	// around an NVIDIA driver bug where glVertexAttribPointer() is thread-hostile
	// (ie., simultaneous GL work in unrelated contexts can cause the driver
	// to free() memory that was never malloc()-ed).
	GLuint create_vec2_vao(const std::set<GLint> &attribute_indices,
	                       GLuint vbo_num);
	void release_vec2_vao(const GLuint vao_num);

	// Informs the ResourcePool that the current context is going away soon,
	// and that any resources held for it in the freelist should be deleted.
	//
	// You do not need to do this for the last context; the regular destructor
	// will take care of that. This means that if you only ever use one
	// thread/context, you never need to call this function.
	void clean_context();

private:
	// Delete the given program and both its shaders.
	void delete_program(GLuint program_num);

	// Deletes all FBOs for the given context that belong to deleted textures.
	void cleanup_unlinked_fbos(void *context);

	// Remove FBOs off the end of the freelist for <context>, until it
	// is no more than <max_length> elements long.
	void shrink_fbo_freelist(void *context, size_t max_length);

	// Same, for VAOs.
	void shrink_vao_freelist(void *context, size_t max_length);

	// Increment the refcount, or take it off the freelist if it's zero.
	void increment_program_refcount(GLuint program_num);

	// If debugging is on, output shader to a temporary file, for easier debugging.
	void output_debug_shader(const std::string &shader_src, const std::string &suffix);

	// For a new program that's not a clone of anything, insert it into the right
	// structures: Give it a refcount, and set up the program_masters / program_instances lists.
	void add_master_program(GLuint program_num);

	// Link the given vertex and fragment shaders into a full GLSL program.
	// See compile_glsl_program() for explanation of <fragment_shader_outputs>.
	static GLuint link_program(GLuint vs_obj,
	                           GLuint fs_obj,
	                           const std::vector<std::string>& fragment_shader_outputs);

	static GLuint link_compute_program(GLuint cs_obj);

	// Protects all the other elements in the class.
	pthread_mutex_t lock;

	size_t program_freelist_max_length, texture_freelist_max_bytes, fbo_freelist_max_length, vao_freelist_max_length;
		
	// A mapping from vertex/fragment shader source strings to compiled program number.
	std::map<std::pair<std::string, std::string>, GLuint> programs;

	// A mapping from compute shader source string to compiled program number.
	std::map<std::string, GLuint> compute_programs;

	// A mapping from compiled program number to number of current users.
	// Once this reaches zero, the program is taken out of this map and instead
	// put on the freelist (after which it may be deleted).
	std::map<GLuint, int> program_refcount;

	// A mapping from program number to vertex and fragment shaders.
	// Contains everything needed to re-link the program.
	struct ShaderSpec {
		GLuint vs_obj, fs_obj;
		std::vector<std::string> fragment_shader_outputs;
	};
	std::map<GLuint, ShaderSpec> program_shaders;

	struct ComputeShaderSpec {
		GLuint cs_obj;
	};
	std::map<GLuint, ComputeShaderSpec> compute_program_shaders;

	// For each program, a list of other programs that are exactly like it.
	// By default, will only contain the program itself, but due to cloning
	// (see use_glsl_program()), may grow. Programs are taken off this list
	// while they are in use (by use_glsl_program()).
	std::map<GLuint, std::stack<GLuint>> program_instances;

	// For each program, the master program that created it
	// (inverse of program_instances).
	std::map<GLuint, GLuint> program_masters;

	// A list of programs that are no longer in use, most recently freed first.
	// Once this reaches <program_freelist_max_length>, the last element
	// will be deleted.
	std::list<GLuint> program_freelist;

	struct Texture2D {
		GLint internal_format;
		GLsizei width, height;
		GLsync no_reuse_before = nullptr;
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

	static const unsigned num_fbo_attachments = 4;
	struct FBO {
		GLuint fbo_num;
		// GL_INVALID_INDEX means associated to a texture that has since been deleted.
		// 0 means the output isn't bound.
		GLuint texture_num[num_fbo_attachments];
	};

	// For each context, a mapping from FBO number to format details. This is
	// filled if the FBO is given out to a client or on the freelist, but
	// not if it is deleted from the freelist.
	std::map<std::pair<void *, GLuint>, FBO> fbo_formats;
	typedef std::map<std::pair<void *, GLuint>, FBO>::iterator FBOFormatIterator;

	// For each context, a list of all FBOs that are released but not freed
	// (most recently freed first). Once this reaches <fbo_freelist_max_length>,
	// the last element will be deleted.
	//
	// We store iterators directly into <fbo_format> for efficiency.
	std::map<void *, std::list<FBOFormatIterator>> fbo_freelist;

	// Very similar, for VAOs.
	struct VAO {
		GLuint vao_num;
		std::set<GLint> attribute_indices;
		GLuint vbo_num;
	};
	std::map<std::pair<void *, GLuint>, VAO> vao_formats;
	typedef std::map<std::pair<void *, GLuint>, VAO>::iterator VAOFormatIterator;
	std::map<void *, std::list<VAOFormatIterator>> vao_freelist;

	// See the caveats at the constructor.
	static size_t estimate_texture_size(const Texture2D &texture_format);
};

}  // namespace movit

#endif  // !defined(_MOVIT_RESOURCE_POOL_H)
