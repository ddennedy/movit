#ifndef _MOVIT_DEINTERLACE_EFFECT_H
#define _MOVIT_DEINTERLACE_EFFECT_H 1

// YADIF deinterlacing filter (original by Michael Niedermayer, in MPlayer).
//
// Good deinterlacing is very hard. YADIF, despite its innocious-sounding
// name (Yet Another DeInterlacing Filter) is probably the most commonly
// used (non-trivial) deinterlacing filter in the open-source world.
// It works by trying to fill in the missing lines from neighboring ones
// (spatial interpolation), and then constrains that estimate within an
// interval found from previous and next frames (temporal interpolation).
// It's not very fast, even in GPU implementation, but 1080i60 -> 1080p60
// realtime conversion is well within range for a mid-range GPU.
//
// The inner workings of YADIF are poorly documented; implementation details
// are generally explained the .frag file. However, a few things should be
// mentioned here: YADIF has two modes, with and without a “spatial interlacing
// check” which basically allows more temporal change in areas of high detail.
// (The variant with the check corresponds to the original's modes 0 and 1, and
// the variant without to modes 2 and 3. The remaining difference is whether it
// is frame-doubling or not, which in Movit is up to the driver, not the
// filter.)
//
// Neither mode is perfect by any means. If the spatial check is off, the
// filter possesses the potentially nice quality that a static picture
// deinterlaces exactly to itself. (If it's on, there's some flickering
// on very fine vertical detail. The picture is nice and stable if no such
// detail is present, though.) But then, certain patterns, like horizontally
// scrolling text, leaves residues. Both have issues with diagonal lines at
// certain angles leaving stray pixels, although in practical applications,
// YADIF is pretty good.
//
// In general, having the spatial check on (the default) is the safe choice.
// However, if you are reasonably certain that the image comes from a video source
// (ie., no graphical overlays), or if the case of still images is particularly
// important for you (e.g., slides from a laptop), you could turn it off.
// It is slightly faster, although in practice, it does not mean all that much.
// You need to decide before finalize(), as the choice gets compiled into the shader.
//
// YADIF needs five fields as input; the previous two, the current one, and
// then the two next ones. (By convention, they come in that order, although if
// you reverse them, it doesn't matter, as the filter is symmetric. It _does_
// matter if you change the ordering in any other way, though.) They need to be
// of the same resolution, or the effect will assert-fail. If you cannot supply
// this, you could simply reuse the current field for previous/next as
// required; it won't be optimal in any way, but it also won't blow up on you.
//
// This requirement to “see the future” will mean you have an extra full frame
// of delay (33.3 ms at 60i, 40 ms at 50i). You will also need to tell the
// filter for each and every invocation if the current field (ie., the one in
// the middle input) is a top or bottom field (neighboring fields have opposite
// parity, so all the others are implicit).

#include <epoxy/gl.h>
#include <memory>
#include <string>

#include "effect.h"

namespace movit {

class DeinterlaceComputeEffect;

class DeinterlaceEffect : public Effect {
public:
	DeinterlaceEffect();
	std::string effect_type_id() const override { return "DeinterlaceEffect"; }
	std::string output_fragment_shader() override;

	// Replaces itself with DeinterlaceComputeEffect if compute shaders are supported.
	// Otherwise, does nothing.
	void rewrite_graph(EffectChain *graph, Node *self) override;
	bool set_int(const std::string &key, int value) override;

	void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num) override;

	// First = before previous, second = previous, third = current,
	// fourth = next, fifth = after next. These are treated symmetrically,
	// though.
	//
	// Note that if you have interlaced _frames_ and not _fields_, you will
	// need to pull them apart first, for instance with SliceEffect.
	unsigned num_inputs() const override { return 5; }
	bool needs_texture_bounce() const override { return true; }
	bool changes_output_size() const override { return true; }

	AlphaHandling alpha_handling() const override { return INPUT_PREMULTIPLIED_ALPHA_KEEP_BLANK; }

	void inform_input_size(unsigned input_num, unsigned width, unsigned height) override;
	void get_output_size(unsigned *width, unsigned *height,
	                     unsigned *virtual_width, unsigned *virtual_height) const override;

	enum FieldPosition { TOP = 0, BOTTOM = 1 };

private:
	// If compute shaders are supported, contains the actual effect.
	// If not, nullptr.
	std::unique_ptr<DeinterlaceComputeEffect> compute_effect_owner;
	DeinterlaceComputeEffect *compute_effect = nullptr;

	unsigned widths[5], heights[5];

	// See file-level comment for explanation of this option.
	bool enable_spatial_interlacing_check;

	// Which field the current input (the middle one) is.
	FieldPosition current_field_position;

	// Offset for one pixel in the horizontal direction (1/width).
	float inv_width;

	// Vertical resolution of the output.
	float num_lines;

	// All of these offsets are vertical texel offsets; they are needed to adjust
	// for the changed texel center as the number of lines double, and depend on
	// <current_field_position>.

	// For sampling unchanged lines from the current field.
	float self_offset;

	// For evaluating the low-pass filter (in the current field). Four taps.
	float current_offset[2];

	// For evaluating the high-pass filter (in the previous and next fields).
	// Five taps, but evaluated twice since there are two fields.
	float other_offset[3];
};

// A compute shader implementation of DeinterlaceEffect. It saves a bunch of loads
// since it can share them between neighboring pixels (and also does not need
// texture bounce), so it has the potential to be faster, although exactly how
// much depends on your chain and other factors. DeinterlaceEffect will
// automatically become a proxy to DeinterlaceComputeEffect if your system
// supports compute shaders.
class DeinterlaceComputeEffect : public Effect {
public:
	DeinterlaceComputeEffect();
	std::string effect_type_id() const override { return "DeinterlaceComputeEffect"; }
	std::string output_fragment_shader() override;

	void set_gl_state(GLuint glsl_program_num, const std::string &prefix, unsigned *sampler_num) override;

	unsigned num_inputs() const override { return 5; }
	bool changes_output_size() const override { return true; }
	bool is_compute_shader() const override { return true; }
	void get_compute_dimensions(unsigned output_width, unsigned output_height,
	                            unsigned *x, unsigned *y, unsigned *z) const override;

	AlphaHandling alpha_handling() const override { return INPUT_PREMULTIPLIED_ALPHA_KEEP_BLANK; }

	void inform_input_size(unsigned input_num, unsigned width, unsigned height) override;
	void get_output_size(unsigned *width, unsigned *height,
	                     unsigned *virtual_width, unsigned *virtual_height) const override;

	enum FieldPosition { TOP = 0, BOTTOM = 1 };

private:
	unsigned widths[5], heights[5];

	// See file-level comment for explanation of this option.
	bool enable_spatial_interlacing_check;

	// Which field the current input (the middle one) is.
	FieldPosition current_field_position;

	// Offset for one pixel in the horizontal and verticla direction (1/width, 1/height).
	float inv_width, inv_height;

	// For evaluating the low-pass filter (in the current field). Four taps.
	float current_field_vertical_offset;
};

}  // namespace movit

#endif // !defined(_MOVIT_DEINTERLACE_EFFECT_H)
