/*
 * drummachine.c
 *
 * Part of libtt (the integer amplifier library)
 *
 * Copyright (C) 2012 Daniel Thompson <daniel@redfelineninja.org.uk> 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <string.h>

#include "librfn.h"
#include "libtt.h"
#include "libtt/drummachine.h"

// core samples (at 23KHz)
#include "lib23k/hhc_Rock_b.h"
#include "lib23k/kick_Dry_b.h"
#include "lib23k/sn_Wet_b.h"

#define TT_DRUMMACHINE_PATTERN_LENGTH 0
#define TT_DRUMMACHINE_DIVISIONS_PER_BEAT 1
#define TT_DRUMMACHINE_PATTERN 2

#define TT_DRUMMACHINE_VOICE_LENGTH ((int) lengthof(kick_Dry_b))

enum {
	__ = 0,
	BA = 0x01,
	SN = 0x02,
	CH = 0x10,
};
uint8_t basic[] = {
	4,					// length of pattern
	1,					// divisions per beat
	CH|BA|__,				// beat 1
	CH|__|SN,				// beat 2
	CH|BA|__,				// beat 3
	CH|__|SN,				// beat 4
};

uint8_t basic8[] = {
	8,					// length of pattern
	2,					// divisions per beat
	CH|BA|__,				// beat 1
	CH|__|__,
	CH|__|SN,				// beat 2
	CH|__|__,
	CH|BA|__,				// beat 3
	CH|__|__,
	CH|__|SN,				// beat 4
	CH|__|__,
};

uint8_t swing8[] = {
	12,					// length of pattern
	3,					// divisions per beat
	CH|BA|__,				// beat 1
	__|__|__,
	CH|__|__,
	CH|__|SN,				// beat 2
	__|__|__,
	CH|__|__,
	CH|BA|__,				// beat 3
	__|__|__,
	CH|__|__,
	CH|__|SN,				// beat 4
	__|__|__,
	CH|__|__,
};

uint8_t augmented8[] = {
	8,					// length of pattern
	2,					// divisions per beat
	CH|BA|__,				// beat 1
	CH|__|__,
	CH|__|SN,				// beat 2
	CH|__|__,
	CH|BA|__,				// beat 3
	CH|BA|__,
	CH|__|SN,				// beat 4
	CH|BA|__,
};

uint8_t *patterns[] = {
	basic,
	basic8,
	swing8,
	augmented8
};

#define UNUSED_SAMPLE hhc_Rock_b
int8_t *samples[8] = {
	kick_Dry_b,				// 0x01
	sn_Wet_b,				// 0x02
	UNUSED_SAMPLE,
	UNUSED_SAMPLE,
	hhc_Rock_b,				// 0x10
	UNUSED_SAMPLE,
	UNUSED_SAMPLE,
	UNUSED_SAMPLE,
};

static uint8_t *lookup_pattern(tt_drummachine_t *dm)
{
	int pattern_index = TTASINT(dm->controls[TT_TAG2ID(TT_DRUMMACHINE_CONTROL_PATTERN)]);
	if (pattern_index >= lengthof(patterns))
		pattern_index = 0;
	return patterns[pattern_index];
}

void tt_drummachine_init(tt_drummachine_t *dm, tt_context_t *ctx)
{
	memset(dm, 0, sizeof(*dm));
	dm->ctx = ctx;

	tt_biquad_init(&dm->resampler, ctx);

	// design a collection of different reconstruction filters with slightly
	// different cut-offs and Q values (used to randomly vary the timbre)
	for (int i=0; i<16; i++) {
		int shfreq = 9400 + (i*50);
		ttspl_t q = TTADD(TTFLOAT(0.55), TTMINT(TTFLOAT(0.15), i&3));

		tt_biquad_lowpass(&dm->resampler, shfreq, q);
		dm->coefficients[i] = dm->resampler.coeff;
	}

	dm->seed = RAND31_VAR_INIT;

	// default values for controls
	dm->controls[TT_TAG2ID(TT_DRUMMACHINE_CONTROL_BPM)] = TTINT(120);
	dm->controls[TT_TAG2ID(TT_DRUMMACHINE_CONTROL_PATTERN)] = TTINT(0);
	dm->controls[TT_TAG2ID(TT_DRUMMACHINE_CONTROL_PATTERN_MAX)] = TTINT(lengthof(patterns));

}

void tt_drummachine_finalize(tt_drummachine_t *dm)
{
}

tt_generic_new(drummachine);
tt_generic_delete(drummachine);

void tt_drummachine_setup(tt_drummachine_t *dm)
{
	// apply the change
	ttspl_t beats_per_minute = dm->controls[TT_TAG2ID(TT_DRUMMACHINE_CONTROL_BPM)];
	uint8_t *pattern = lookup_pattern(dm);

	// We struggle to maintain numeric accuracy here. The trick to maintain
	// accuracy is knowing that TTRAISE(TLDIV(...)) does not truncate to 32 bits
	ttspl_t beats_per_second = TTDINT(beats_per_minute, 60);
	unsigned int samples_per_second = dm->ctx->sampling_frequency / 2;
	tlspl_t samples_per_beat = TTRAISE(TLDIV(TLINT(samples_per_second), beats_per_second));
	tlspl_t samples_per_division = TLDINT(samples_per_beat, pattern[TT_DRUMMACHINE_DIVISIONS_PER_BEAT]);

	dm->division_reload = TLASINT(samples_per_division);
	dm->division_counter = 0;

	dm->pattern_start = pattern + TT_DRUMMACHINE_PATTERN;
	dm->pattern_end = dm->pattern_start + pattern[TT_DRUMMACHINE_PATTERN_LENGTH];
	dm->pattern_pointer = dm->pattern_start;
}

tt_generic_get_control(drummachine, TT_DRUMMACHINE_CONTROL_MIN, TT_DRUMMACHINE_CONTROL_MAX);

void tt_drummachine_set_control(tt_drummachine_t *dm, tt_drummachine_control_t ctrl, ttspl_t val)
{
	assert(ctrl >= TT_DRUMMACHINE_CONTROL_MIN && ctrl < TT_DRUMMACHINE_CONTROL_MAX);

	/* this value is read-only */
	if (ctrl == TT_DRUMMACHINE_CONTROL_PATTERN_MAX)
		return;

	dm->controls[TT_TAG2ID(ctrl)] = val;
	tt_drummachine_setup(dm);
}

tt_generic_enum_control(drummachine, TT_DRUMMACHINE_CONTROL_MIN, TT_DRUMMACHINE_CONTROL_MAX);

unsigned int tt_drummachine_samples_until_next_beat(tt_drummachine_t *dm)
{
	uint8_t *pattern = lookup_pattern(dm);
	uint8_t divisions_per_beat = pattern[TT_DRUMMACHINE_DIVISIONS_PER_BEAT];

	// the divison variables are prior to the resampler so must be multipled by two
	unsigned int samples_until_next_division = 2*dm->division_counter + dm->cold_sample;
	unsigned int samples_per_division = 2*dm->division_reload;
	unsigned int pattern_offset = dm->pattern_pointer - dm->pattern_start;
	unsigned int divisions_so_far = pattern_offset % divisions_per_beat;
	unsigned int divisions_until_next_beat = (0 == divisions_so_far ? 0 :
			divisions_per_beat - divisions_so_far);

	return (divisions_until_next_beat * samples_per_division) +
			samples_until_next_division;
}

/*!
 * Produce the raw sequence of mixed and timed drum samples.
 *
 * The main algorithm operates purely on integers (even on f.p. builds, only
 * the final return statement converts things to ttspl_t).
 */
static inline ttspl_t tt_drummachine_ministep(tt_drummachine_t *dm)
{
	int32_t spl;

	if (0 == dm->division_counter) {
		// trigger the voices
		for (int i=0; i<lengthof(dm->voice_pointer); i++)
			if ((1 << i) & *(dm->pattern_pointer)) {
				dm->voice_pointer[i] = TT_DRUMMACHINE_VOICE_LENGTH - 64;
				dm->voice_pointer[i] += rand31_r(&dm->seed) & 255;
			}


		// restart the division  counter and move to the next part of the pattern
		dm->division_counter = dm->division_reload;
		if (++dm->pattern_pointer >= dm->pattern_end)
			dm->pattern_pointer = dm->pattern_start;
	} else {
		dm->division_counter--;
	}

	// process the voices
	spl = 0;
	for (int i=0; i<lengthof(dm->voice_pointer); i++) {
		if (dm->voice_pointer[i]) {
			int j = TT_DRUMMACHINE_VOICE_LENGTH - dm->voice_pointer[i];

			if (j >= 0)
				spl += samples[i][j] * dm->voice_pointer[i];

			dm->voice_pointer[i]--;
		}
	}
	// 256 comes from the fact the underlying samples are 8-bit and the output
	// must be 16-bit. It is is a magic number but not one that suites soft coding.
	spl /= (TT_DRUMMACHINE_VOICE_LENGTH / 256);

	// saturate
	if (spl > 32767)
		spl = 32767;
	else if (spl < -32768)
		spl = -32768;

	// done
	return TTS16LE(spl);
}

/*!
 * Produce a single drum machine sample.
 *
 * This function is essentially just a 2x integer resampler acting on the
 * sequence produced tt_drummachine_ministep().
 */
ttspl_t tt_drummachine_step(tt_drummachine_t *dm)
{
	ttspl_t spl;

	bool hot_sample = !dm->cold_sample;
	dm->cold_sample = hot_sample;

	if (hot_sample) {
		spl = tt_drummachine_ministep(dm);

		if (0 == dm->division_counter) {
			// update the filter coefficients just before the
			// beat is triggered... this is when the filter is
			// at its most quiet so we shouldn't get a pop
			// the change
			dm->resampler.coeff = dm->coefficients[
			                rand31_r(&dm->seed) % lengthof(dm->coefficients)];
		}
	} else {
		spl = 0;
	}

	return tt_biquad_step(&dm->resampler, spl);
}

tt_generic_output(drummachine);
