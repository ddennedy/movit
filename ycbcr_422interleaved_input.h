#ifndef _MOVIT_YCBCR_422INTERLEAVED_INPUT_H
#define _MOVIT_YCBCR_422INTERLEAVED_INPUT_H 1

// YCbCr422InterleavedInput is for handling 4:2:2 interleaved 8-bit Y'CbCr,
// which you can get from e.g. certain capture cards. (Most other Y'CbCr
// encodings are planar, which is handled by YCbCrInput.) Currently we only
// handle the UYVY variant, although YUY2 should be easy to support if needed.
//
// Horizontal chroma placement is freely choosable as with YCbCrInput,
// but BT.601 (which at least DeckLink claims to conform to, under the
// name CCIR 601) seems to specify chroma positioning to the far left
// (that is 0.0); BT.601 Annex 1 (page 7) says “C R and C B samples co-sited
// with odd (1st, 3rd, 5th, etc.) Y samples in each line”, and I assume they do
// not start counting from 0 when they use the “1st” moniker.
//
// Interpolation is bilinear as in YCbCrInput (done by the GPU's normal
// scaling, except for the Y channel which of course needs some fiddling),
// and is done in non-linear light (since that's what everything specifies,
// except Rec. 2020 lets you choose between the two). A higher-quality
// choice would be to use a single pass of ResampleEffect to scale the
// chroma, but for now we are consistent between the two.
//
// There is a disparity between the interleaving and the way OpenGL typically
// expects to sample. In lieu of accessible hardware support (a lot of hardware
// supports native interleaved 4:2:2 sampling, but OpenGL drivers seem to
// rarely support it), we simply upload the same data twice; once as a
// full-width RG texture (from which we sample luma) and once as a half-width
// RGBA texture (from which we sample chroma). We throw away half of the color
// channels each time, so bandwidth is wasted, but it makes for a very
// uncomplicated shader.
//
// Note that if you can shuffle your data around very cheaply on the CPU
// (say, while you're decoding it out of some other buffer anyway),
// regular YCbCrInput with YCBCR_INPUT_SPLIT_Y_AND_CBCR will probably be
// more efficient, as it doesn't need this bandwidth waste.

#include <epoxy/gl.h>
#include <string>

#include "effect.h"
#include "effect_chain.h"
#include "image_format.h"
#include "input.h"
#include "ycbcr.h"

namespace movit {

class ResourcePool;

class YCbCr422InterleavedInput : public Input {
public:
	// <ycbcr_format> must be consistent with 4:2:2 sampling; specifically:
	//
	//  * chroma_subsampling_x must be 2.
	//  * chroma_subsampling_y must be 1.
	//
	// <width> must obviously be an even number. It is the true width of the image
	// in pixels, ie., the number of horizontal luma samples.
	YCbCr422InterleavedInput(const ImageFormat &image_format,
	                         const YCbCrFormat &ycbcr_format,
				 unsigned width, unsigned height);
	~YCbCr422InterleavedInput();

	std::string effect_type_id() const override { return "YCbCr422InterleavedInput"; }

	bool can_output_linear_gamma() const override { return false; }
	AlphaHandling alpha_handling() const override { return OUTPUT_BLANK_ALPHA; }

	std::string output_fragment_shader() override;

	// Uploads the texture if it has changed since last time.
	void set_gl_state(GLuint glsl_program_num, const std::string& prefix, unsigned *sampler_num) override;

	unsigned get_width() const override { return width; }
	unsigned get_height() const override { return height; }
	Colorspace get_color_space() const override { return image_format.color_space; }
	GammaCurve get_gamma_curve() const override { return image_format.gamma_curve; }
	bool can_supply_mipmaps() const override { return false; }

	// Tells the input where to fetch the actual pixel data. Note that if you change
	// this data, you must either call set_pixel_data() again (using the same pointer
	// is fine), or invalidate_pixel_data(). Otherwise, the texture won't be re-uploaded
	// on subsequent frames.
	//
	// The data can either be a regular pointer (if pbo==0), or a byte offset
	// into a PBO. The latter will allow you to start uploading the texture data
	// asynchronously to the GPU, if you have any CPU-intensive work between the
	// call to set_pixel_data() and the actual rendering. Also, since we upload
	// the data twice, using a PBO can save texture upload bandwidth. In either case,
	// the pointer (and PBO, if set) has to be valid at the time of the render call.
	void set_pixel_data(const unsigned char *pixel_data, GLuint pbo = 0)
	{
		this->pixel_data = pixel_data;
		this->pbo = pbo;
		invalidate_pixel_data();
	}

	void invalidate_pixel_data();

	void set_pitch(unsigned pitch)
	{
		assert(pitch % ycbcr_format.chroma_subsampling_x == 0);
		pitches[CHANNEL_LUMA] = pitch;
		pitches[CHANNEL_CHROMA] = pitch / ycbcr_format.chroma_subsampling_x;
		invalidate_pixel_data();
	}

	void inform_added(EffectChain *chain) override
	{
		resource_pool = chain->get_resource_pool();
	}

	bool set_int(const std::string& key, int value) override;

private:
	ImageFormat image_format;
	YCbCrFormat ycbcr_format;
	GLuint pbo;

	// Luma texture is 0, chroma texture is 1.
	enum Channel {
		CHANNEL_LUMA,
		CHANNEL_CHROMA
	};
	GLuint texture_num[2];
	GLuint widths[2];
	unsigned pitches[2];

	unsigned width, height;
	const unsigned char *pixel_data;
	ResourcePool *resource_pool;

	GLint uniform_tex_y, uniform_tex_cbcr;
};

}  // namespace movit

#endif  // !defined(_MOVIT_YCBCR_422INTERLEAVED_INPUT_H)
