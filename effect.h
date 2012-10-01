#ifndef _EFFECT_H
#define _EFFECT_H 1

#include <map>
#include <string>

// Can alias on a float[3].
struct RGBTriplet {
	RGBTriplet(float r, float g, float b)
		: r(r), g(g), b(b) {}

	float r, g, b;
};

class Effect {
public: 
	virtual bool needs_linear_light() { return true; }
	virtual bool needs_srgb_primaries() { return true; }
	virtual bool needs_many_samples() { return false; }
	virtual bool needs_mipmaps() { return false; }

	virtual std::string output_convenience_uniforms();
	virtual std::string output_glsl() = 0;

	// Neither of these take ownership.
	bool set_int(const std::string&, int value);
	bool set_float(const std::string &key, float value);
	bool set_vec3(const std::string &key, const float *values);

protected:
	// Neither of these take ownership.
	void register_int(const std::string &key, int *value);
	void register_float(const std::string &key, float *value);
	void register_vec3(const std::string &key, float *values);
	
private:
	std::map<std::string, int *> params_int;
	std::map<std::string, float *> params_float;
	std::map<std::string, float *> params_vec3;
};

#endif // !defined(_EFFECT_H)
