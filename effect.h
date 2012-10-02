#ifndef _EFFECT_H
#define _EFFECT_H 1

#include <map>
#include <string>

#include <GL/gl.h>

// Can alias on a float[2].
struct Point2D {
	Point2D(float x, float y)
		: x(x), y(y) {}

	float x, y;
};

// Can alias on a float[3].
struct RGBTriplet {
	RGBTriplet(float r, float g, float b)
		: r(r), g(g), b(b) {}

	float r, g, b;
};

// Convenience functions that deal with prepending the prefix.
void set_uniform_float(GLuint glsl_program_num, const std::string &prefix, const std::string &key, float value);
void set_uniform_vec2(GLuint glsl_program_num, const std::string &prefix, const std::string &key, const float *values);
void set_uniform_vec3(GLuint glsl_program_num, const std::string &prefix, const std::string &key, const float *values);

class Effect {
public: 
	virtual bool needs_linear_light() { return true; }
	virtual bool needs_srgb_primaries() { return true; }
	virtual bool needs_many_samples() { return false; }
	virtual bool needs_mipmaps() { return false; }

	virtual std::string output_convenience_uniforms();
	virtual std::string output_vertex_shader();
	virtual std::string output_fragment_shader() = 0;

	virtual void set_uniforms(GLuint glsl_program_num, const std::string& prefix);

	// Neither of these take ownership.
	bool set_int(const std::string&, int value);
	bool set_float(const std::string &key, float value);
	bool set_vec2(const std::string &key, const float *values);
	bool set_vec3(const std::string &key, const float *values);

protected:
	// Neither of these take ownership.
	void register_int(const std::string &key, int *value);
	void register_float(const std::string &key, float *value);
	void register_vec2(const std::string &key, float *values);
	void register_vec3(const std::string &key, float *values);
	
private:
	std::map<std::string, int *> params_int;
	std::map<std::string, float *> params_float;
	std::map<std::string, float *> params_vec2;
	std::map<std::string, float *> params_vec3;
};

#endif // !defined(_EFFECT_H)
