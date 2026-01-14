/* 
 * Derived from
 *
 *  tslib/tests/ts_print_mt.c
 *
 *  Copyright (C) 2017 Martin Kepplinger
 *
 * This file is part of tslib.
 *
 * ts_print_mt is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * ts_print_mt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *  Modified by Blaise Dias, to provide a service function for touch
 *  screens.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include "timing.h"

#include "touch_screen.h"

#if defined (__FreeBSD__)

#include <dev/evdev/input.h>
#define TS_HAVE_EVDEV

#elif defined (__linux__)

#include <linux/input.h>
#define TS_HAVE_EVDEV

#endif

#ifdef TS_HAVE_EVDEV
#include <sys/ioctl.h>
#endif

#include <tslib.h>

#ifndef ABS_MT_SLOT /* < 2.6.36 kernel headers */
# define ABS_MT_SLOT             0x2f    /* MT slot being modified */
#endif

static int errfn(const char *fmt, va_list ap)
{
    return vfprintf(stderr, fmt, ap);
}

static int openfn(const char *path, int flags,
          void *user_data __attribute__((unused)))
{
    return open(path, flags);
}

static int poll_events = 0;
void stop_touch_screen_service(void) {
    poll_events = 0;
}

int touch_screen_service(touch_screen_svc_config* config)
{
    struct tsdev *ts;
    char *tsdevice = NULL;
    struct ts_sample_mt **samp_mt = NULL;
#ifdef TS_HAVE_EVDEV
    struct input_absinfo slot;
#endif
    int32_t max_slots = 1;
    int ret, i, j;
    int read_samples = 1;
    short raw = 0;
    struct ts_lib_version_data *ver = ts_libversion();

#ifndef TSLIB_VERSION_MT /* < 1.10 */
    printf("You are running an old version of tslib. Please upgrade.\n");
#endif

    ts_error_fn = errfn;

#ifdef TSLIB_VERSION_OPEN_RESTRICTED
    printf("#defined TSLIB_VERSION_OPEN_RESTRICTED\n");
    if (ver->features & TSLIB_VERSION_OPEN_RESTRICTED)
        ts_open_restricted = openfn;
#endif

    ts = ts_setup(tsdevice, 1);

    if (!ts) {
        perror("ts_setup");
        return errno;
    }

    printf("libts %06X opened device %s\n",
           ver->version_num, ts_get_eventpath(ts));

#ifdef TS_HAVE_EVDEV
    printf("#defined TS_HAVE_EVDEV\n");
    if (ioctl(ts_fd(ts), EVIOCGABS(ABS_MT_SLOT), &slot) < 0) {
        perror("ioctl EVIOGABS");
        ts_close(ts);
        return errno;
    }

    max_slots = slot.maximum + 1 - slot.minimum;
    printf("max_slots %d\n", max_slots);
#endif

    samp_mt = malloc(read_samples * sizeof(struct ts_sample_mt *));
    if (!samp_mt) {
        ts_close(ts);
        return -ENOMEM;
    }
    for (i = 0; i < read_samples; i++) {
        samp_mt[i] = calloc(max_slots, sizeof(struct ts_sample_mt));
        if (!samp_mt[i]) {
            free(samp_mt);
            ts_close(ts);
            return -ENOMEM;
        }
    }

    printf("read_samples=%d max_slots=%d\n", read_samples, max_slots);
    int down = 0;
    poll_events = 1;
    while (poll_events) {
        if (raw)
            ret = ts_read_raw_mt(ts, samp_mt, max_slots, read_samples);
        else
            ret = ts_read_mt(ts, samp_mt, max_slots, read_samples);

        if (ret < 0) {
            if (config != NULL) {
                sleep_milli_seconds(config->sleeptime);
            }
            continue;
        }

        for (j = 0; j < ret; j++) {
            for (i = 0; i < max_slots; i++) {
                if (!(samp_mt[j][i].valid & TSLIB_MT_VALID))
                    continue;

                if ( samp_mt[j][i].pressure ) {
                    if (down == 0) {
                        if (config == NULL) {
                            printf("FINGER_DOWN %d,%d\n", samp_mt[j][i].x, samp_mt[j][i].y);
                        } else {
                            config->down(samp_mt[j][i].x, samp_mt[j][i].y);
                        }
                    } else {
                        if (config == NULL) {
                            printf("FINGER_MOTION %d,%d\n", samp_mt[j][i].x, samp_mt[j][i].y);
                        } else {
                            config->motion(samp_mt[j][i].x, samp_mt[j][i].y);
                        }
                    }
                } else if (down) {
                    if (config == NULL) {
                        printf("FINGER_UP %d,%d\n", samp_mt[j][i].x, samp_mt[j][i].y);
                    } else {
                        config->up(samp_mt[j][i].x, samp_mt[j][i].y);
                    }
                }
                down = samp_mt[j][i].pressure;
            }
        }
    }

    free(samp_mt);
    ts_close(ts);
    return 0;
}
