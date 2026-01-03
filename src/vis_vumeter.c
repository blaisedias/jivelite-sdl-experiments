/*
** Copyright 2010 Logitech. All Rights Reserved.
** Copyright 2025 Blaise Dias
**
** This file is licensed under BSD. Please see the LICENSE file for details.
*/


#include "types.h"
#include "visualizer.h"

#define VUMETER_DEFAULT_SAMPLE_WINDOW 1024 * 2

static long long RMS_MAP[] = {
	   0,    2,    5,    7,   10,   21,   33,   45,   57,   82,
	 108,  133,  159,  200,  242,  284,  326,  387,  448,  509,
	 570,  652,  735,  817,  900, 1005, 1111, 1217, 1323, 1454,
	1585, 1716, 1847, 2005, 2163, 2321, 2480, 2666, 2853, 3040,
	3227, 3414, 3601, 3788, 3975, 4162, 4349, 4536, 4755, 5000,
};

extern void (*vol_printf)(char *format, ...);

int visualizer_vumeter(int* levels) {
	long long sample_accumulator[2];
	int16_t *ptr;
	s16_t sample;
	s32_t sample_sq;
	size_t i, num_samples, samples_until_wrap;

	int offs;

	num_samples = VUMETER_DEFAULT_SAMPLE_WINDOW;

	sample_accumulator[0] = 0;
	sample_accumulator[1] = 0;

	vis_check();

	if (vis_get_playing()) {

		vis_lock();

		offs = vis_get_buffer_idx() - (num_samples * 2);
		while (offs < 0) offs += vis_get_buffer_len();

		ptr = vis_get_buffer() + offs;
		samples_until_wrap = vis_get_buffer_len() - offs;

		for (i=0; i<num_samples; i++) {
			sample = (*ptr++) >> 8;
			sample_sq = sample * sample;
			sample_accumulator[0] += sample_sq;

			sample = (*ptr++) >> 8;
			sample_sq = sample * sample;
			sample_accumulator[1] += sample_sq;

			samples_until_wrap -= 2;
			if (samples_until_wrap <= 0) {
				ptr = vis_get_buffer();
				samples_until_wrap = vis_get_buffer_len();
			}
		}

		vis_unlock();
	}

	sample_accumulator[0] /= num_samples;
	sample_accumulator[1] /= num_samples;
//    printf("%08lld %08lld ", sample_accumulator[0], sample_accumulator[1]);

    for(int indx =0; indx < 2; ++indx) {
        levels[indx] = 0;
        for (int level = 49; level >=0; --level) {
            if (sample_accumulator[indx] > RMS_MAP[level]) {
                levels[indx] = level;
//                vol_printf("%02d %08lld %08lld ", level, sample_accumulator[indx], RMS_MAP[level]);
                vol_printf("%02d %08lld %08lld ", level, sample_accumulator[indx], RMS_MAP[level]);
                break;
            }
        }
    }

	return 1;
}
