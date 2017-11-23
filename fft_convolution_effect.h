#ifndef _MOVIT_FFT_CONVOLUTION_EFFECT_H
#define _MOVIT_FFT_CONVOLUTION_EFFECT_H 1

// FFTConvolutionEffect applies an arbitrary 2D convolution between the input image
// and a convolution kernel (assumed to be much smaller than the image).
// It does this convolution using multiple smaller FFTs and an algorithm called
// overlap-discard (also known as overlap-save) to achieve much higher
// efficiency than direct evaluation of the convolution, at some expense of
// accuracy.
//
// FFTConvolutionEffect follows the usual convention for convolution, which is that
// you sample from the origin pixel, and then up and to the left from that. This means
// that (in horizontal 1D) [1 0 0 0 0 ...] would be an identity transform, and that
// [0 1 0 0 0 ...] would mean sampling one pixel to the left of the origin, which
// effectively move would move the image one pixel to the right.
//
// The basic idea of the acceleration comes from the the convolution theorem
// (which holds in any number of dimensions), namely that FFT(A ⊙ B) =
// FFT(A) * FFT(B), where ⊙ is circular convolution and * is pointwise
// multiplication. This means that A ⊙ B = IFFT(FFT(A) * FFT(B)), and since
// we can do a 2D FFT in O(n² log n), this is asymptotically better than
// direct convolution, which is O(n²m²) (where m is the size of the convolution
// kernel). However, the convolution theorem is rarely _directly_ applicable,
// for two reasons:
//
//  - ⊙ is _circular_ convolution, which means that inputs are taken to
//    repeat (wrap around), which is rarely what you want.
//  - A and B must be the same size, which means that to convolve a 1280x720
//    kernel with a 64x64 kernel, you need to zero pad the 64x64 kernel and
//    then do _two_ full 1280x720 FFTs (one for each of A and B).
//
// The first problem is solved by adding m-1 zero pixels (horizontally
// and vertically) as padding, and then discarding the result of those pixels.
// This causes the output to be identical to a non-circular convolution.
//
// The second is slightly more tricky, and there are multiple ways of solving
// it. The one that appears to be the most suitable to suitable for GPU use,
// and the one that is used here, is overlap-discard (more commonly but less
// precisely known as overlap-save). In overlap-discard, the input is broken
// up into multiple equally-sized slices which are then FFTed and convolved
// with the kernel individually. (The kernel must still be zero padded to the
// same size as the slice, but this is typically much smaller then the full
// picture.) As before, the pad area contains data that's essentially junk,
// which is thrown away when the slices are put together again.
//
// The optimal slice size is a tradeoff. More slices means more space wasted
// for padding, since the padding is the same no matter the slice size,
// but fewer slices means we need to do larger FFTs (although fewer of them).
// There's no exact closed formula for this, especially since the 2D case
// makes things a bit more tricky with ordering of the X versus Y passes,
// so we simply try all possible sizes and orderings, attempting to estimate
// roughly how much each operation will cost. The result isn't perfect, though;
// FFTW has a mode for actually measuring, which they claim improves speeds
// by ~2x over simple estimation, but they also have much more freedom in
// their execution model than we do.
//
// The output _size_ of a convolution can be defined in a couple of different
// ways; in a sense, what's the most reasonable is using only the central part
// of the result (the mode “valid” in MATLAB/Octave), since that is the only
// one not used by any edge pixels. (FFTConvolutionEffect assumes normal Movit
// edge pixel behavior, which is to repeat the outermost pixels.) You could also
// keep all the output pixels (“full” in MATLAB/Octave), which is nicely symmetric.
// However, for video processing, typically what you want is to have the _same_
// output size as input size, so we crop to the input size. This means that
// you'll get some of the edge-affected pixels but not all, but it's usually
// an okay tradeoff.
//
// FFTConvolutionEffect does not do any actual pixel work by itself; it
// rewrites itself into a long chain of SliceEffect, FFTPassEffect, FFTInput
// and ComplexModulationEffect to do its bidding. Note that currently, due to
// Movit limitations, we need to know the number of FFT passes at finalize()
// time, which in turn means you cannot change image or kernel size on the fly.

#include <assert.h>
#include <epoxy/gl.h>
#include <string>

#include "effect.h"
#include "fft_input.h"

namespace movit {

class PaddingEffect;

class FFTConvolutionEffect : public Effect {
public:
	FFTConvolutionEffect(int input_width, int input_height, int convolve_width, int convolve_height);
	~FFTConvolutionEffect();
	std::string effect_type_id() const override { return "FFTConvolutionEffect"; }
	std::string output_fragment_shader() override { assert(false); }
	void rewrite_graph(EffectChain *graph, Node *self) override;

	// See FFTInput::set_pixel_data().
	void set_convolution_kernel(const float *pixel_data)
	{
		assert(fft_input);
		fft_input->set_pixel_data(pixel_data);
	}

private:
	int input_width, input_height;
	int convolve_width, convolve_height;

	// Both of these are owned by us if owns_effects is true (before finalize()),
	// and otherwise owned by the EffectChain.
	FFTInput *fft_input;
	PaddingEffect *crop_effect;
	bool owns_effects;
};

}  // namespace movit

#endif // !defined(_MOVIT_FFT_CONVOLUTION_EFFECT_H)
