#ifndef _MOVIT_DECONVOLUTION_SHARPEN_EFFECT_H
#define _MOVIT_DECONVOLUTION_SHARPEN_EFFECT_H 1

// DeconvolutionSharpenEffect is an effect that sharpens by way of deconvolution
// (i.e., trying to reverse the blur kernel, as opposed to just boosting high
// frequencies), more specifically by FIR Wiener filters. It is the same
// algorithm as used by the (now largely abandoned) Refocus plug-in for GIMP,
// and I suspect the same as in Photoshop's “Smart Sharpen” filter.
// The implementation is, however, distinct from either.
//
// The effect gives generally better results than unsharp masking, but can be very
// GPU intensive, and requires a fair bit of tweaking to get good results without
// ringing and/or excessive noise. It should be mentioned that for the larger
// convolutions (e.g. R approaching 10), we should probably move to FFT-based
// convolution algorithms, especially as Mesa's shader compiler starts having
// problems compiling our shader.
//
// We follow the same book as Refocus was implemented from, namely
//
//   Jain, Anil K.: “Fundamentals of Digital Image Processing”, Prentice Hall, 1988.

#include <epoxy/gl.h>
#include <Eigen/Dense>
#include <string>

#include "effect.h"

namespace movit {

class DeconvolutionSharpenEffect : public Effect {
public:
	DeconvolutionSharpenEffect();
	virtual ~DeconvolutionSharpenEffect();
	std::string effect_type_id() const override { return "DeconvolutionSharpenEffect"; }
	std::string output_fragment_shader() override;

	// Samples a lot of times from its input.
	bool needs_texture_bounce() const override { return true; }

	void inform_input_size(unsigned input_num, unsigned width, unsigned height) override
	{
		this->width = width;
		this->height = height;
	}

	void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num) override;
	AlphaHandling alpha_handling() const override { return INPUT_PREMULTIPLIED_ALPHA_KEEP_BLANK; }

private:
	// Input size.
	unsigned width, height;

	// The maximum radius of the (de)convolution kernel.
	// Note that since this extends both ways, and we also have a center element,
	// the actual convolution matrix will be (2R + 1) x (2R + 1).
	//
	// Must match the definition in the shader, and as such, cannot be set once
	// the chain has been finalized.
	int R;

	// The parameters. Typical OK values are circle_radius = 2, gaussian_radius = 0
	// (ie., blur is assumed to be a 2px circle), correlation = 0.95, and noise = 0.01.
	// Note that once the radius starts going too far past R, you will get nonsensical results.
	float circle_radius, gaussian_radius, correlation, noise;

	// The deconvolution kernel, and the parameters last time we did an update.
	Eigen::MatrixXf g;
	int last_R;
	float last_circle_radius, last_gaussian_radius, last_correlation, last_noise;

	float *uniform_samples;
	
	void update_deconvolution_kernel();
};

}  // namespace movit

#endif // !defined(_MOVIT_DECONVOLUTION_SHARPEN_EFFECT_H)
