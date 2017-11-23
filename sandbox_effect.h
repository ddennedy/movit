#ifndef _MOVIT_SANDBOX_EFFECT_H
#define _MOVIT_SANDBOX_EFFECT_H 1

// This effect, by default, does nothing.
//
// But imagine all the cool things you can make it do! Thus, the SandboxEffect
// is intended to be a sandbox for you to have a place to write your test or
// throwaway code. When you're happy, you can do a bit of search and replace
// to give it a proper name and its own place in the build system.

#include <epoxy/gl.h>
#include <string>

#include "effect.h"

namespace movit {

class SandboxEffect : public Effect {
public:
	SandboxEffect();
	std::string effect_type_id() const override { return "SandboxEffect"; }
	std::string output_fragment_shader() override;

	void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num) override;

private:
	float parm;
};

}  // namespace movit

#endif // !defined(_MOVIT_SANDBOX_EFFECT_H)
